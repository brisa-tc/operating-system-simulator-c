#include <estructura.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ─── Variables globales ───────────────────────────────────────────────────────

int segment_max_size;
char* allocation_strategy;
int instruction_delay;
int compaction_delay;
char* scripts_basepath;
char* puerto_escucha;
char* log_level;

t_list* lista_contextos;
t_list* lista_segmentos;
t_list* lista_memory_sticks;
t_list* lista_huecos;
t_list* lista_cpus;

int socket_kernel_scheduler = -1;
int socket_swap = -1;
int swap_block_size = 0;
int swap_file_size = 0;
int swap_block_count = 0;
char* bitmap_swap_storage = NULL;
t_bitarray* bitmap_swap = NULL;

t_config* config;
t_log* logger;

pthread_mutex_t mutex_contextos;
pthread_mutex_t mutex_segmentos;
pthread_mutex_t mutex_segmentos_global;
pthread_mutex_t mutex_huecos;
pthread_mutex_t mutex_memory_sticks;
pthread_mutex_t mutex_cpus;
pthread_mutex_t mutex_swap;
pthread_mutex_t mutex_segmentos_en_swap;

// ─── Config ───────────────────────────────────────────────────────────────────

t_config* iniciar_config(char* path_config) {
    config = config_create(path_config);
    if (config == NULL) {
        printf("Error: No se pudo cargar el config '%s'\n", path_config);
        exit(EXIT_FAILURE);
    }
    return config;
}

void cargar_config(t_config* config_ptr) {
    log_level           = strdup(config_get_string_value(config_ptr, "LOG_LEVEL"));
    segment_max_size    = config_get_int_value(config_ptr, "SEGMENT_MAX_SIZE");
    allocation_strategy = strdup(config_get_string_value(config_ptr, "ALLOCATION_STRATEGY"));
    instruction_delay   = config_get_int_value(config_ptr, "INSTRUCTION_DELAY");
    compaction_delay    = config_get_int_value(config_ptr, "COMPACTION_DELAY");
    scripts_basepath    = strdup(config_get_string_value(config_ptr, "SCRIPTS_BASEPATH"));
    puerto_escucha      = strdup(config_get_string_value(config_ptr, "PUERTO_ESCUCHA"));
}

// ─── Logger ───────────────────────────────────────────────────────────────────

void iniciar_logger() {
    logger = log_create(
        "log_kernel_memory.log",
        "kernel_memory",
        true,
        log_level_from_string(log_level)
    );
    if (logger == NULL) {
        printf("Error al crear logger\n");
        exit(EXIT_FAILURE);
    }
    log_info(logger, "Iniciando Kernel Memory...");
    log_debug(logger, "Configuracion cargada - Max Size: %d | Estrategia: %s",
              segment_max_size, allocation_strategy);
    log_info(logger, "Servidor escuchando en el puerto %s", puerto_escucha);
}

// ─── Inicialización de estructuras ────────────────────────────────────────────

void iniciar_estructuras() {
    lista_contextos     = list_create();
    lista_segmentos     = list_create();
    lista_memory_sticks = list_create();
    lista_huecos        = list_create();
    lista_cpus          = list_create();

    pthread_mutex_init(&mutex_contextos,        NULL);
    pthread_mutex_init(&mutex_segmentos,        NULL);
    pthread_mutex_init(&mutex_segmentos_global, NULL);
    pthread_mutex_init(&mutex_huecos,           NULL);
    pthread_mutex_init(&mutex_memory_sticks,    NULL);
    pthread_mutex_init(&mutex_cpus,             NULL);
    pthread_mutex_init(&mutex_swap,             NULL);
    pthread_mutex_init(&mutex_segmentos_en_swap, NULL);
}

void inicializar_bitmap_swap(int block_size, int file_size)
{
    if (block_size <= 0 || file_size <= 0) {
        log_warning(logger, "No se puede inicializar bitmap SWAP con block_size=%d file_size=%d",
                    block_size, file_size);
        return;
    }

    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap != NULL) {
        bitarray_destroy(bitmap_swap);
        bitmap_swap = NULL;
    }

    free(bitmap_swap_storage);

    swap_block_size = block_size;
    swap_file_size = file_size;
    swap_block_count = file_size / block_size;

    int bytes_bitmap = (swap_block_count + 7) / 8;
    bitmap_swap_storage = calloc(bytes_bitmap, sizeof(char));
    if (bitmap_swap_storage == NULL) {
        swap_block_count = 0;
        pthread_mutex_unlock(&mutex_swap);
        log_error(logger, "No se pudo reservar memoria para el bitmap de SWAP");
        return;
    }

    bitmap_swap = bitarray_create_with_mode(bitmap_swap_storage, bytes_bitmap, LSB_FIRST);
    if (bitmap_swap == NULL) {
        free(bitmap_swap_storage);
        bitmap_swap_storage = NULL;
        swap_block_count = 0;
        pthread_mutex_unlock(&mutex_swap);
        log_error(logger, "No se pudo crear el bitmap de SWAP");
        return;
    }

    pthread_mutex_unlock(&mutex_swap);

    log_info(logger, "## Bitmap SWAP inicializado - Bloques: %d", swap_block_count);
}

int obtener_bloque_swap_libre(void)
{
    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap == NULL) {
        pthread_mutex_unlock(&mutex_swap);
        return -1;
    }

    for (int i = 0; i < swap_block_count; i++) {
        if (!bitarray_test_bit(bitmap_swap, i)) {
            pthread_mutex_unlock(&mutex_swap);
            return i;
        }
    }

    pthread_mutex_unlock(&mutex_swap);
    return -1;
}

bool hay_bloques_swap_disponibles(int cantidad)
{
    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap == NULL || cantidad < 0) {
        pthread_mutex_unlock(&mutex_swap);
        return false;
    }

    int libres = 0;
    for (int i = 0; i < swap_block_count; i++) {
        if (!bitarray_test_bit(bitmap_swap, i)) {
            libres++;
        }
    }

    pthread_mutex_unlock(&mutex_swap);
    return libres >= cantidad;
}

void marcar_bloque_swap_ocupado(int bloque)
{
    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap != NULL && bloque >= 0 && bloque < swap_block_count) {
        bitarray_set_bit(bitmap_swap, bloque);
    }

    pthread_mutex_unlock(&mutex_swap);
}

void marcar_bloque_swap_libre(int bloque)
{
    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap != NULL && bloque >= 0 && bloque < swap_block_count) {
        bitarray_clean_bit(bitmap_swap, bloque);
    }

    pthread_mutex_unlock(&mutex_swap);
}

int reservar_bloque_swap(void)
{
    pthread_mutex_lock(&mutex_swap);

    if (bitmap_swap == NULL) {
        pthread_mutex_unlock(&mutex_swap);
        return -1;
    }

    for (int i = 0; i < swap_block_count; i++) {
        if (!bitarray_test_bit(bitmap_swap, i)) {
            bitarray_set_bit(bitmap_swap, i);
            pthread_mutex_unlock(&mutex_swap);
            return i;
        }
    }

    pthread_mutex_unlock(&mutex_swap);
    return -1;
}

void liberar_bloques_swap(t_list* bloques)
{
    if (bloques == NULL)
        return;

    for (int i = 0; i < list_size(bloques); i++) {
        int bloque = *(int*)list_get(bloques, i);
        marcar_bloque_swap_libre(bloque);
    }
}
