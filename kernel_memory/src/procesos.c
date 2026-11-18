#include "procesos.h"
#include "segmentos.h"
#include "registro.h"
#include <utils/lecturaEscritura.h>
#include <utils/comunicacion.h>
#include <stdlib.h>
#include <string.h>

static void destruir_segmento_swap(void* elemento) //punto 4 de la lista hecho 
{
    t_segmento_swap* segmento_swap = elemento;
    if (segmento_swap == NULL)
        return;

    liberar_bloques_swap(segmento_swap->bloques);
    list_destroy_and_destroy_elements(segmento_swap->bloques, free);
    free(segmento_swap);
}

static bool swap_escribir_bloque(int bloque, char* contenido, int tamanio)
{
    if (socket_swap == -1 || bloque < 0 || tamanio <= 0)
        return false;

    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SWAP_WRITE_BLOCK;
    agregar_a_paquete(paquete, &bloque, sizeof(int));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, contenido, tamanio);
    enviar_paquete(paquete, socket_swap);
    eliminar_paquete(paquete);

    return recibir_operacion(socket_swap) == MEM_OK;
}

static char* swap_leer_bloque(int bloque, int* tamanio)
{
    if (socket_swap == -1 || bloque < 0 || tamanio == NULL)
        return NULL;

    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SWAP_READ_BLOCK;
    agregar_a_paquete(paquete, &bloque, sizeof(int));
    enviar_paquete(paquete, socket_swap);
    eliminar_paquete(paquete);

    int op = recibir_operacion(socket_swap);
    if (op != SWAP_READ_BLOCK)
        return NULL;

    t_list* valores = recibir_paquete(socket_swap);
    if (list_size(valores) < 1) {
        list_destroy_and_destroy_elements(valores, free);
        return NULL;
    }

    void* bloque_leido = list_get(valores, 0);
    int tamanio_bloque = swap_block_size;
    char* contenido = malloc(tamanio_bloque);
    if (contenido != NULL)
        memcpy(contenido, bloque_leido, tamanio_bloque);

    *tamanio = tamanio_bloque;
    list_destroy_and_destroy_elements(valores, free);
    return contenido;
}

// ─── Crear contexto ───────────────────────────────────────────────────────────

t_contexto* crear_contexto(int pid, char* path)
{
    t_contexto* nuevo = malloc(sizeof(t_contexto));
    if (nuevo == NULL)
        return NULL;

    nuevo->pid = pid;
    memset(&(nuevo->registros), 0, sizeof(t_registros));

    nuevo->tabla_segmentos = malloc(sizeof(t_tabla_segmentos));
    if (nuevo->tabla_segmentos == NULL) {
        free(nuevo);
        return NULL;
    }
    nuevo->tabla_segmentos->pid       = pid;
    nuevo->tabla_segmentos->segmentos = list_create();

    nuevo->segmentos_en_swap = list_create();
    if (nuevo->segmentos_en_swap == NULL) {
        list_destroy(nuevo->tabla_segmentos->segmentos);
        free(nuevo->tabla_segmentos);
        free(nuevo);
        return NULL;
    }

    nuevo->path_instrucciones = strdup(path);

    pthread_mutex_lock(&mutex_contextos);
    list_add(lista_contextos, nuevo);
    pthread_mutex_unlock(&mutex_contextos);

    log_info(logger, "## PID: %d - Proceso Creado", pid);
    return nuevo;
}

// ─── Buscar contexto ──────────────────────────────────────────────────────────

t_contexto* buscar_contexto(int pid)
{
    pthread_mutex_lock(&mutex_contextos);

    t_contexto* resultado = NULL;
    for (int i = 0; i < list_size(lista_contextos); i++) {
        t_contexto* ctx = list_get(lista_contextos, i);
        if (ctx->pid == pid) {
            resultado = ctx;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_contextos);
    return resultado;
}

// ─── Crear proceso ────────────────────────────────────────────────────────────

bool crear_proceso(int pid, char* path)
{
    if (buscar_contexto(pid) != NULL)
        return false;

    t_contexto* nuevo = crear_contexto(pid, path);
    if (nuevo == NULL)
        return false;

    iniciar_registros(&nuevo->registros);
    return true;
}

// ─── Finalizar proceso ────────────────────────────────────────────────────────

void finalizar_proceso(int pid)
{
    t_contexto* ctx = buscar_contexto(pid);
    if (ctx == NULL) {
        log_warning(logger, "## PID: %d - No encontrado para finalizar", pid);
        return;
    }

    while (!list_is_empty(ctx->tabla_segmentos->segmentos)) {
        t_segmento* seg = list_get(ctx->tabla_segmentos->segmentos, 0);
        eliminar_segmento(ctx, seg->id);
    }

    pthread_mutex_lock(&mutex_contextos);
    list_remove_element(lista_contextos, ctx);
    pthread_mutex_unlock(&mutex_contextos);

    list_destroy(ctx->tabla_segmentos->segmentos);
    free(ctx->tabla_segmentos);
    pthread_mutex_lock(&mutex_segmentos_en_swap);
    list_destroy_and_destroy_elements(ctx->segmentos_en_swap, destruir_segmento_swap);
    pthread_mutex_unlock(&mutex_segmentos_en_swap);
    free(ctx->path_instrucciones);
    free(ctx);

    log_info(logger, "## PID: %d - Proceso Finalizado", pid);

    if (socket_kernel_scheduler != -1)
        enviar_codigo(MEM_AMPLIADA, socket_kernel_scheduler);
}

// ─── Suspender proceso ────────────────────────────────────────────────────────

bool suspender_proceso(int pid)
{
    t_contexto* ctx = buscar_contexto(pid);
    if (ctx == NULL)
        return false;

    if (socket_swap == -1 || swap_block_size <= 0) {
        log_warning(logger, "## PID: %d - No se puede suspender sin SWAP conectado", pid);
        return false;
    }

    t_list* segmentos_a_suspender = list_create();
    pthread_mutex_lock(&mutex_segmentos);
    for (int i = 0; i < list_size(ctx->tabla_segmentos->segmentos); i++) {
        t_segmento* seg = list_get(ctx->tabla_segmentos->segmentos, i);
        int* id = malloc(sizeof(int));
        if (id != NULL) {
            *id = seg->id;
            list_add(segmentos_a_suspender, id);
        }
    }
    pthread_mutex_unlock(&mutex_segmentos);

    for (int i = 0; i < list_size(segmentos_a_suspender); i++) {
        int id_segmento = *(int*)list_get(segmentos_a_suspender, i);

        t_segmento* seg = NULL;
        pthread_mutex_lock(&mutex_segmentos);
        for (int j = 0; j < list_size(ctx->tabla_segmentos->segmentos); j++) {
            t_segmento* candidato = list_get(ctx->tabla_segmentos->segmentos, j);
            if (candidato->id == id_segmento) {
                seg = candidato;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_segmentos);

        if (seg == NULL)
            continue;

        uint32_t direccion_logica = id_segmento * segment_max_size;
        char* contenido_segmento = leer_datos(pid, direccion_logica, seg->tamanio);
        if (contenido_segmento == NULL) {
            list_destroy_and_destroy_elements(segmentos_a_suspender, free);
            return false;
        }

        int cantidad_bloques = (seg->tamanio + swap_block_size - 1) / swap_block_size;
        if (!hay_bloques_swap_disponibles(cantidad_bloques)) {
            free(contenido_segmento);
            list_destroy_and_destroy_elements(segmentos_a_suspender, free);
            return false;
        }

        t_segmento_swap* segmento_swap = malloc(sizeof(t_segmento_swap));
        segmento_swap->id_segmento = id_segmento;
        segmento_swap->tamanio_segmento = seg->tamanio;
        segmento_swap->bloques = list_create();

        for (int bloque_idx = 0; bloque_idx < cantidad_bloques; bloque_idx++) {
            int bloque = reservar_bloque_swap();
            if (bloque == -1) {
                free(contenido_segmento);
                destruir_segmento_swap(segmento_swap);
                list_destroy_and_destroy_elements(segmentos_a_suspender, free);
                return false;
            }

            char* contenido_bloque = calloc(swap_block_size, sizeof(char));
            int offset = bloque_idx * swap_block_size;
            int bytes_a_copiar = segmento_swap->tamanio_segmento - offset;
            if (bytes_a_copiar > swap_block_size)
                bytes_a_copiar = swap_block_size;
            memcpy(contenido_bloque, contenido_segmento + offset, bytes_a_copiar);

            if (!swap_escribir_bloque(bloque, contenido_bloque, swap_block_size)) {
                free(contenido_bloque);
                marcar_bloque_swap_libre(bloque);
                free(contenido_segmento);
                destruir_segmento_swap(segmento_swap);
                list_destroy_and_destroy_elements(segmentos_a_suspender, free);
                return false;
            }

            free(contenido_bloque);
            int* bloque_guardado = malloc(sizeof(int));
            *bloque_guardado = bloque;
            list_add(segmento_swap->bloques, bloque_guardado);
        }

        pthread_mutex_lock(&mutex_segmentos_en_swap);
        list_add(ctx->segmentos_en_swap, segmento_swap);
        pthread_mutex_unlock(&mutex_segmentos_en_swap);
        eliminar_segmento(ctx, id_segmento);
        log_info(logger, "## PID: %d - Segmento %d suspendido en SWAP - Bloques: %d",
                 pid,
                 id_segmento,
                 cantidad_bloques);
        free(contenido_segmento);
    }

    list_destroy_and_destroy_elements(segmentos_a_suspender, free);
    return true;
}

// ─── Des-suspender proceso ────────────────────────────────────────────────────

bool des_suspender_proceso(int pid)
{
    t_contexto* ctx = buscar_contexto(pid);
    if (ctx == NULL)
        return false;

    while (1) {
        pthread_mutex_lock(&mutex_segmentos_en_swap);
        if (list_is_empty(ctx->segmentos_en_swap)) {
            pthread_mutex_unlock(&mutex_segmentos_en_swap);
            break;
        }
        t_segmento_swap* segmento_swap = list_get(ctx->segmentos_en_swap, 0);
        pthread_mutex_unlock(&mutex_segmentos_en_swap);

        t_segmento* nuevo = crear_segmento(ctx, segmento_swap->id_segmento, segmento_swap->tamanio_segmento);
        if (nuevo == NULL)
            return false;

        char* contenido_segmento = calloc(segmento_swap->tamanio_segmento + 1, sizeof(char));
        if (contenido_segmento == NULL)
            return false;

        bool lectura_ok = true;
        for (int i = 0; i < list_size(segmento_swap->bloques); i++) {
            int bloque = *(int*)list_get(segmento_swap->bloques, i);
            int tamanio_leido = 0;
            char* contenido_bloque = swap_leer_bloque(bloque, &tamanio_leido);
            if (contenido_bloque == NULL) {
                lectura_ok = false;
                break;
            }

            int offset = i * swap_block_size;
            int bytes_a_copiar = segmento_swap->tamanio_segmento - offset;
            if (bytes_a_copiar > tamanio_leido)
                bytes_a_copiar = tamanio_leido;
            memcpy(contenido_segmento + offset, contenido_bloque, bytes_a_copiar);
            free(contenido_bloque);
        }

        if (!lectura_ok) {
            free(contenido_segmento);
            pthread_mutex_lock(&mutex_segmentos_en_swap);
            list_remove(ctx->segmentos_en_swap, 0);
            pthread_mutex_unlock(&mutex_segmentos_en_swap);
            destruir_segmento_swap(segmento_swap);
            return false;
        }

        uint32_t direccion_logica = segmento_swap->id_segmento * segment_max_size;
        bool ok = escribir_datos(pid, direccion_logica, segmento_swap->tamanio_segmento, contenido_segmento);
        free(contenido_segmento);

        if (!ok) {
            pthread_mutex_lock(&mutex_segmentos_en_swap);
            list_remove(ctx->segmentos_en_swap, 0);
            pthread_mutex_unlock(&mutex_segmentos_en_swap);
            destruir_segmento_swap(segmento_swap);
            return false;
        }

        liberar_bloques_swap(segmento_swap->bloques);

        pthread_mutex_lock(&mutex_segmentos_en_swap);
        list_remove(ctx->segmentos_en_swap, 0);
        pthread_mutex_unlock(&mutex_segmentos_en_swap);

        log_info(logger, "## PID: %d - Segmento %d restaurado desde SWAP",
                 pid,
                 segmento_swap->id_segmento);
        destruir_segmento_swap(segmento_swap);
    }

    return true;
}

// ─── Traducir dirección lógica a física ───────────────────────────────────────

int traducir_direccion(int pid, uint32_t dir_logica, int tamanio, uint32_t* dir_fisica)
{
    t_contexto* ctx = buscar_contexto(pid);
    if (ctx == NULL) {
        log_error(logger, "## PID: %d - Traducción: Contexto no encontrado para dir_logica=%u",
                  pid, dir_logica);
        return -1;
    }

    int      num_segmento = dir_logica / segment_max_size;
    uint32_t desp         = dir_logica % segment_max_size;

    log_debug(logger, "## PID: %d - Traducción: dir_logica=%u → segmento=%d, desplazamiento=%u, tamaño=%d",
              pid, dir_logica, num_segmento, desp, tamanio);

    t_segmento* seg = NULL;
    pthread_mutex_lock(&mutex_segmentos);
    for (int i = 0; i < list_size(ctx->tabla_segmentos->segmentos); i++) {
        t_segmento* s = list_get(ctx->tabla_segmentos->segmentos, i);
        if (s->id == num_segmento) {
            seg = s;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_segmentos);

    if (seg == NULL) {
        log_error(logger, "## PID: %d - Traducción: Segmento %d no encontrado en tabla de segmentos",
                  pid, num_segmento);
        return -1;
    }

    if ((desp + (uint32_t)tamanio) > seg->tamanio) {
        log_error(logger, "## PID: %d - Traducción: SEG_FAULT - segmento %d (base=%u, tamaño=%u) "
                          "no cubre dir_logica=%u + tamaño=%d = %u",
                  pid, num_segmento, seg->base, seg->tamanio,
                  dir_logica, tamanio, desp + (uint32_t)tamanio);
        return -1;
    }

    *dir_fisica = seg->base + desp;
    log_debug(logger, "## PID: %d - Traducción exitosa: dir_logica=%u → segmento %d (base=%u, desp=%u) → dir_fisica=%u",
              pid, dir_logica, num_segmento, seg->base, desp, *dir_fisica);
    return 0;
}

static t_memoryStick_info* buscar_memory_stick_por_direccion(uint32_t dir_fisica)
{
    t_memoryStick_info* resultado = NULL;

    pthread_mutex_lock(&mutex_memory_sticks);

    for (int i = 0; i < list_size(lista_memory_sticks); i++) {
        t_memoryStick_info* ms = list_get(lista_memory_sticks, i);
        uint32_t inicio = ms->base;
        uint32_t fin = ms->base + ms->tamanio;

        if (dir_fisica >= inicio && dir_fisica < fin) {
            resultado = ms;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_memory_sticks);
    return resultado;
}

static int bytes_hasta_fin_de_memory_stick(t_memoryStick_info* ms, uint32_t dir_fisica, int bytes_restantes)
{
    uint32_t fin_stick = ms->base + ms->tamanio;
    uint32_t disponibles = fin_stick - dir_fisica;

    return disponibles < (uint32_t)bytes_restantes
        ? (int)disponibles
        : bytes_restantes;
}

// ─── Leer datos ───────────────────────────────────────────────────────────────

char* leer_datos(int pid, uint32_t dir_logica, int tamanio)
{
    uint32_t dir_fisica = 0;
    if (traducir_direccion(pid, dir_logica, tamanio, &dir_fisica) == -1) {
        log_warning(logger, "## PID: %d - SEG_FAULT en lectura - Dir. Logica: %u",
                    pid, dir_logica);
        return NULL;
    }

    char* contenido = malloc(tamanio + 1);
    if (contenido == NULL)
        return NULL;

    int leidos = 0;
    while (leidos < tamanio) {
        uint32_t dir_actual = dir_fisica + leidos;
        t_memoryStick_info* ms = buscar_memory_stick_por_direccion(dir_actual);
        if (ms == NULL) {
            log_warning(logger, "## PID: %d - Lectura sin Memory Stick para Dir. Física: %u",
                        pid, dir_actual);
            free(contenido);
            return NULL;
        }

        int bytes_a_leer = bytes_hasta_fin_de_memory_stick(ms, dir_actual, tamanio - leidos);
        t_lectura lectura = {
            .direccion = (int)(dir_actual - ms->base),
            .tamanio = bytes_a_leer
        };

        enviar_lectura(&lectura, ms->socket);

        int respuesta_op = recibir_operacion(ms->socket);
        if (respuesta_op != LECTURA) {
            log_warning(logger, "## PID: %d - Respuesta de lectura inesperada: %d", pid, respuesta_op);
            free(contenido);
            return NULL;
        }

        t_lectura_respuesta* respuesta = recibir_respuesta_lectura(ms->socket);
        if (respuesta == NULL || respuesta->tamanio != bytes_a_leer) {
            eliminar_respuesta_lectura(respuesta);
            free(contenido);
            return NULL;
        }
        memcpy(contenido + leidos, respuesta->contenido, bytes_a_leer);
        eliminar_respuesta_lectura(respuesta);

        leidos += bytes_a_leer;
    }
    contenido[tamanio] = '\0';

    log_info(logger, "## PID: %d - Lectura - Dir. Física: %u - Tamaño: %d",
             pid, dir_fisica, tamanio);

    return contenido;
}

// ─── Escribir datos ───────────────────────────────────────────────────────────

bool escribir_datos(int pid, uint32_t dir_logica, int tamanio, char* contenido)
{
    uint32_t dir_fisica = 0;
    if (traducir_direccion(pid, dir_logica, tamanio, &dir_fisica) == -1) {
        log_warning(logger, "## PID: %d - SEG_FAULT en escritura - Dir. Logica: %u",
                    pid, dir_logica);
        return false;
    }

    int escritos = 0;
    while (escritos < tamanio) {
        uint32_t dir_actual = dir_fisica + escritos;
        t_memoryStick_info* ms = buscar_memory_stick_por_direccion(dir_actual);
        if (ms == NULL) {
            log_warning(logger, "## PID: %d - Escritura sin Memory Stick para Dir. Física: %u",
                        pid, dir_actual);
            return false;
        }

        int bytes_a_escribir = bytes_hasta_fin_de_memory_stick(ms, dir_actual, tamanio - escritos);
        char* contenido_parcial = malloc(bytes_a_escribir);
        if (contenido_parcial == NULL)
            return false;

        memcpy(contenido_parcial, contenido + escritos, bytes_a_escribir);
        t_escritura escritura = {
            .direccion = (int)(dir_actual - ms->base),
            .tamanio = bytes_a_escribir,
            .contenido = contenido_parcial
        };

        enviar_escritura(&escritura, ms->socket);
        free(contenido_parcial);

        int respuesta_op = recibir_operacion(ms->socket);
        if (respuesta_op != ESCRITURA) {
            log_warning(logger, "## PID: %d - Respuesta de escritura inesperada: %d", pid, respuesta_op);
            return false;
        }

        t_escritura_respuesta* respuesta = recibir_respuesta_escritura(ms->socket);
        bool ok = respuesta->resultado;
        free(respuesta);

        if (!ok)
            return false;

        escritos += bytes_a_escribir;
    }

    log_info(logger, "## PID: %d - Escritura - Dir. Física: %u - Tamaño: %d",
             pid, dir_fisica, tamanio);

    return true;
}

// ─── Espacio libre ────────────────────────────────────────────────────────────

int obtener_espacio_libre()
{
    pthread_mutex_lock(&mutex_huecos);
    int espacio = 0;
    for (int i = 0; i < list_size(lista_huecos); i++) {
        t_hueco* h = list_get(lista_huecos, i);
        espacio += h->tamanio;
    }
    pthread_mutex_unlock(&mutex_huecos);
    log_debug(logger, "Espacio libre total: %d bytes", espacio);
    return espacio;
}

// ─── Ampliar memoria (nuevo memory stick) ─────────────────────────────────────

uint32_t ampliar_memoria_usuario(uint32_t tamanio)
{
    // La base del nuevo hueco es el fin de todo lo que ya existe
    uint32_t base = 0;

    pthread_mutex_lock(&mutex_huecos);
    for (int i = 0; i < list_size(lista_huecos); i++) {
        t_hueco* h = list_get(lista_huecos, i);
        uint32_t fin = h->base + h->tamanio;
        if (fin > base) base = fin;
    }
    pthread_mutex_unlock(&mutex_huecos);

    pthread_mutex_lock(&mutex_segmentos_global);
    for (int i = 0; i < list_size(lista_segmentos); i++) {
        t_segmento* s = list_get(lista_segmentos, i);
        uint32_t fin = s->base + s->tamanio;
        if (fin > base) base = fin;
    }
    pthread_mutex_unlock(&mutex_segmentos_global);

    pthread_mutex_lock(&mutex_huecos);
    t_hueco* nuevo = malloc(sizeof(t_hueco));
    nuevo->base    = base;
    nuevo->tamanio = tamanio;
    list_add(lista_huecos, nuevo);

    bool fusionado = true;
    while (fusionado) {
        fusionado = false;
        for (int i = 0; i < list_size(lista_huecos); i++) {
            t_hueco* h1 = list_get(lista_huecos, i);
            for (int j = 0; j < list_size(lista_huecos); j++) {
                if (i == j) continue;
                t_hueco* h2 = list_get(lista_huecos, j);
                if (h1->base + h1->tamanio == h2->base) {
                    h1->tamanio += h2->tamanio;
                    list_remove(lista_huecos, j);
                    free(h2);
                    fusionado = true;
                    break;
                }
            }
            if (fusionado) break;
        }
    }
    pthread_mutex_unlock(&mutex_huecos);

    return base;
}

// ─── Leer/Escribir desde dirección física (sin traducción lógica) ──────────

static char* leer_fisica(uint32_t dir_fisica, int tamanio)
{
    char* contenido = malloc(tamanio + 1);
    if (contenido == NULL)
        return NULL;

    int leidos = 0;
    while (leidos < tamanio) {
        uint32_t dir_actual = dir_fisica + leidos;
        t_memoryStick_info* ms = buscar_memory_stick_por_direccion(dir_actual);
        if (ms == NULL) {
            log_warning(logger, "## Compactación - Lectura sin Memory Stick para Dir. Física: %u", dir_actual);
            free(contenido);
            return NULL;
        }

        int bytes_a_leer = bytes_hasta_fin_de_memory_stick(ms, dir_actual, tamanio - leidos);
        t_lectura lectura = {
            .direccion = (int)(dir_actual - ms->base),
            .tamanio = bytes_a_leer
        };

        enviar_lectura(&lectura, ms->socket);

        int respuesta_op = recibir_operacion(ms->socket);
        if (respuesta_op != LECTURA) {
            log_warning(logger, "## Compactación - Respuesta de lectura inesperada: %d", respuesta_op);
            free(contenido);
            return NULL;
        }

        t_lectura_respuesta* respuesta = recibir_respuesta_lectura(ms->socket);
        if (respuesta == NULL || respuesta->tamanio != bytes_a_leer) {
            eliminar_respuesta_lectura(respuesta);
            free(contenido);
            return NULL;
        }
        memcpy(contenido + leidos, respuesta->contenido, bytes_a_leer);
        eliminar_respuesta_lectura(respuesta);

        leidos += bytes_a_leer;
    }
    contenido[tamanio] = '\0';

    log_info(logger, "## Compactación - Lectura física - Dir: %u - Tamaño: %d", dir_fisica, tamanio);

    return contenido;
}

static bool escribir_fisica(uint32_t dir_fisica, int tamanio, char* contenido)
{
    int escritos = 0;
    while (escritos < tamanio) {
        uint32_t dir_actual = dir_fisica + escritos;
        t_memoryStick_info* ms = buscar_memory_stick_por_direccion(dir_actual);
        if (ms == NULL) {
            log_warning(logger, "## Compactación - Escritura sin Memory Stick para Dir. Física: %u", dir_actual);
            return false;
        }

        int bytes_a_escribir = bytes_hasta_fin_de_memory_stick(ms, dir_actual, tamanio - escritos);
        char* contenido_parcial = malloc(bytes_a_escribir);
        if (contenido_parcial == NULL)
            return false;

        memcpy(contenido_parcial, contenido + escritos, bytes_a_escribir);
        t_escritura escritura = {
            .direccion = (int)(dir_actual - ms->base),
            .tamanio = bytes_a_escribir,
            .contenido = contenido_parcial
        };

        enviar_escritura(&escritura, ms->socket);
        free(contenido_parcial);

        int respuesta_op = recibir_operacion(ms->socket);
        if (respuesta_op != ESCRITURA) {
            log_warning(logger, "## Compactación - Respuesta de escritura inesperada: %d", respuesta_op);
            return false;
        }

        t_escritura_respuesta* respuesta = recibir_respuesta_escritura(ms->socket);
        bool ok = respuesta->resultado;
        free(respuesta);

        if (!ok)
            return false;

        escritos += bytes_a_escribir;
    }

    log_info(logger, "## Compactación - Escritura física - Dir: %u - Tamaño: %d", dir_fisica, tamanio);

    return true;
}

// ─── Compactar memoria ────────────────────────────────────────────────────────

bool compactar_memoria(void)
{
    // Calcular memoria total (fin del último hueco o segmento)
    uint32_t memoria_total = 0;

    pthread_mutex_lock(&mutex_huecos);
    for (int i = 0; i < list_size(lista_huecos); i++) {
        t_hueco* h = list_get(lista_huecos, i);
        uint32_t fin = h->base + h->tamanio;
        if (fin > memoria_total) memoria_total = fin;
    }
    pthread_mutex_unlock(&mutex_huecos);

    pthread_mutex_lock(&mutex_segmentos);
    pthread_mutex_lock(&mutex_segmentos_global);

    // Recolectar todos los segmentos en una lista temporal ordenada por base
    t_list* todos = list_create();
    for (int i = 0; i < list_size(lista_segmentos); i++) {
        list_add(todos, list_get(lista_segmentos, i));
    }

    // Ordenar por base (bubble sort sobre t_list — sin list_sort para evitar callbacks)
    for (int i = 0; i < list_size(todos) - 1; i++) {
        for (int j = i + 1; j < list_size(todos); j++) {
            t_segmento* a = list_get(todos, i);
            t_segmento* b = list_get(todos, j);
            if (a->base > b->base) {
                list_replace(todos, i, b);
                list_replace(todos, j, a);
            }
        }
    }

    // Reubicar cada segmento al principio, moviendo los datos físicamente en los memory sticks
    uint32_t base_actual = 0;
    for (int i = 0; i < list_size(todos); i++) {
        t_segmento* seg = list_get(todos, i);
        uint32_t old_base = seg->base;

        if (old_base != base_actual) {
            char* data = leer_fisica(old_base, seg->tamanio);
            if (data == NULL) {
                log_error(logger, "## Compactación: error al leer datos del segmento %d desde base %u",
                          seg->id, old_base);
                pthread_mutex_unlock(&mutex_segmentos_global);
                pthread_mutex_unlock(&mutex_segmentos);
                list_destroy(todos);
                return false;
            }

            seg->base = base_actual;

            bool ok = escribir_fisica(seg->base, seg->tamanio, data);
            free(data);
            if (!ok) {
                log_error(logger, "## Compactación: error al escribir datos del segmento %d en base %u",
                          seg->id, seg->base);
                pthread_mutex_unlock(&mutex_segmentos_global);
                pthread_mutex_unlock(&mutex_segmentos);
                list_destroy(todos);
                return false;
            }
        } else {
            seg->base = base_actual;
        }

        base_actual += seg->tamanio;
    }

    pthread_mutex_unlock(&mutex_segmentos_global);
    pthread_mutex_unlock(&mutex_segmentos);

    list_destroy(todos);

    // Reconstruir huecos: un único hueco al final
    pthread_mutex_lock(&mutex_huecos);
    list_destroy_and_destroy_elements(lista_huecos, free);
    lista_huecos = list_create();

    if (base_actual < memoria_total) {
        t_hueco* hueco_final = malloc(sizeof(t_hueco));
        hueco_final->base    = base_actual;
        hueco_final->tamanio = memoria_total - base_actual;
        list_add(lista_huecos, hueco_final);
    }
    pthread_mutex_unlock(&mutex_huecos);

    return true;
}
