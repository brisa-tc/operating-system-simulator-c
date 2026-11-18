#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <utils/comunicacion.h>
#include <utils/identificadores.h>
#include <utils/memorySticks.h>
#include "conexion.h"

t_log *logger;
void *memoria;
int tamanio_memoria;
int memory_delay;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

//TODO: MUTEX para proteger memoria

int main(int argc, char *argv[])
{
    //Inicio de modulo
    t_config *config = config_create(argv[1]);
    int tamanio = atoi(argv[2]);
    tamanio_memoria = tamanio;
    char* log_level = config_get_string_value(config, "LOG_LEVEL");
    memory_delay = config_get_int_value(config, "MEMORY_DELAY");    
    char *ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    char *puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("log_mem_stick.log", "memory_stick", true, log_level_from_string(log_level));

    //logear datos del config
    log_debug(logger, "LOG LEVEL: %s",log_level);
    log_debug(logger, "MEMORY DELAY: %d",memory_delay);
    log_debug(logger, "IP KERNEL MEMORY: %s", ip_kernel_memory);
    log_debug(logger, "PUERTO KERNEL MEMORY: %s", puerto_kernel_memory);
    log_debug(logger, "PUERTO ESCUCHA: %s", puerto_escucha);

    // reservar memoria
    memoria = calloc(1, tamanio);

    /*
    //pruebas memoria
    char *valor = string_duplicate("chauu");
    void *destino = memoria + 50;                             // obtiene un puntero en la direccion indicada
    memcpy(destino, valor, string_length(valor)); // copiar el contenido a esa direccion

    void *origen = memoria + 50;                                // obtiene un puntero en la direccion indicada
    void *leido = malloc(string_length(valor) + 1);
    memcpy(leido, origen, string_length(valor)); // copiar el contenido en esa direccion
    // Supongamos que tu variable se llama 'leido' y es un void*
    char *leido_char = (char *)leido;

    // Ahora puedes usarlo como string
    leido_char[string_length(valor)] = '\0';
    log_debug(logger, "%s", leido_char);
    */

    //conectar a kernel memory
    int socket_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);
    if (socket_kernel_memory == -1)
    {
        log_error(logger, "No se pudo conectar al kernel memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    //enviar_mensaje("Hola, soy MEMORY STICK", socket_kernel_memory);
    enviar_identificador(MENSAJE_MEMORY_STICK, socket_kernel_memory);

    //crear la informacion del memory stick
    t_memoryStick_info *memory_stick = malloc(sizeof(t_memoryStick_info));
    memory_stick->tamanio = tamanio;
    memory_stick->puerto = string_duplicate(puerto_escucha);
    memory_stick->ip = string_duplicate(""); //vacio porque el kernel memory obtendra su ip

    //enviar la informacion del memory stick
    enviar_memory_stick_info(memory_stick, socket_kernel_memory);
    eliminar_memory_stick_info(memory_stick);
    log_info(logger, "## Conectado a Kernel Memory");

    //crear servidor para cpu
    int socket_server = iniciar_servidor(puerto_escucha);
    if (socket_server == -1) {
        log_error(logger, "Parámetros inválidos");
        return EXIT_FAILURE;
    }

    log_info(logger, "Servidor listo para recibir conexiones");

    //crear hilo para esperar pedidos desde kernel memory
    pthread_t thread_kernel_memory;
    pthread_create(&thread_kernel_memory, NULL, esperar_kernel_memory, &socket_kernel_memory);
    pthread_detach(thread_kernel_memory);
    //esperar pedidos desde las cpus
    esperar_cpus(socket_server);

    //liberar memoria
    free(memoria);
    config_destroy(config);
    log_destroy(logger);
    return 0;
}
