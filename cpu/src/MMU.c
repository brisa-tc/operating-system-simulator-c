#include "globals.h"
#include "MMU.h"
#include "conexiones.h"
#include <commons/string.h>
#include <commons/collections/list.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <utils/lecturaEscritura.h>
#include <utils/memorySticks.h>
#include <math.h>
#include <stdio.h>
#include "utils/contexto_utils.h"

bool obtener_info_ms_por_direccion_fisica(uint32_t dir_fisica, int* socket, uint32_t* base, int* tamanio) {
    pthread_mutex_lock(&mutex_memory_sticks);
    for (int i = 0; i < list_size(lista_memory_sticks); i++) {
        t_memoryStick_info* info = list_get(lista_memory_sticks, i);
        if (dir_fisica >= info->base && dir_fisica < info->base + (uint32_t)info->tamanio) {
            *socket = info->socket;
            *base = info->base;
            *tamanio = info->tamanio;
            pthread_mutex_unlock(&mutex_memory_sticks);
            return true;
        }
    }
    pthread_mutex_unlock(&mutex_memory_sticks);
    log_error(logger, "No hay memory stick que maneje la dirección física %u", dir_fisica);
    return false;
}

static int bytes_hasta_fin_ms(uint32_t dir_fisica, int bytes_solicitados, uint32_t base_ms, int tamanio_ms) {
    uint32_t fin_ms = base_ms + (uint32_t)tamanio_ms;
    uint32_t disponibles = fin_ms - dir_fisica;
    return (int)disponibles < bytes_solicitados ? (int)disponibles : bytes_solicitados;
}

static uint32_t valor_para_log(const void* contenido, int tamanio) {
    uint32_t valor = 0;
    int bytes = tamanio < (int)sizeof(uint32_t) ? tamanio : (int)sizeof(uint32_t);
    memcpy(&valor, contenido, bytes);
    return valor;
}

t_resultado_memoria_cpu traducir_rango_logico(
    uint32_t dir_logica,
    int tamanio,
    uint32_t* dir_fisica
) {
    //int tam_segmento = tam_max_segmento_cpu > 0 ? tam_max_segmento_cpu : 256;
    uint32_t num_segmento = dir_logica / tam_max_segmento_cpu;
    uint32_t desplazamiento = dir_logica % tam_max_segmento_cpu;

    pthread_mutex_lock(&mutex_tabla_segmentos);

    t_resultado_memoria_cpu resultado = MEM_CPU_SEG_FAULT;
    bool segmento_encontrado = false;
    for (int i = 0; tabla_segmentos_actual != NULL && i < list_size(tabla_segmentos_actual); i++) {
        t_segmento_contexto* segmento = list_get(tabla_segmentos_actual, i);
        if (segmento->id == (int)num_segmento) {
            segmento_encontrado = true;
            if (tamanio < 0 || desplazamiento + (uint32_t)tamanio > segmento->tamanio) {
                log_error(logger,
                          "SEGMENTATION FAULT: desplazamiento %u + tamaño %d excede el segmento %u de tamaño %u",
                          desplazamiento,
                          tamanio,
                          num_segmento,
                          segmento->tamanio);
                break;
            }
            *dir_fisica = segmento->base + desplazamiento;
            resultado = MEM_CPU_OK;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_tabla_segmentos);

    if (!segmento_encontrado) {
        log_error(logger, "No se encontró el segmento %u en la tabla de segmentos", num_segmento);
    } else if (resultado == MEM_CPU_OK) {
        log_debug(logger, "[MMU] Traducción: dir_lógica=%u → dir_física=%u", dir_logica, *dir_fisica);
    }

    return resultado;
}

t_resultado_memoria_cpu leer_memoria(uint32_t dir_logica, int tamanio_leer, void** contenido_leido) {
    uint32_t dir_fisica;
    t_resultado_memoria_cpu traduccion = traducir_rango_logico(dir_logica, tamanio_leer, &dir_fisica);
    if (traduccion != MEM_CPU_OK)
        return traduccion;

    void* contenido = malloc(tamanio_leer + 1);
    if (contenido == NULL)
        return MEM_CPU_ERROR;

    int leidos = 0;
    while (leidos < tamanio_leer) {
        uint32_t dir_actual = dir_fisica + leidos;

        int socket_ms;
        uint32_t base_ms;
        int tamanio_ms;
        if (!obtener_info_ms_por_direccion_fisica(dir_actual, &socket_ms, &base_ms, &tamanio_ms)) {
            free(contenido);
            return MEM_CPU_ERROR;
        }

        int bytes_a_leer = bytes_hasta_fin_ms(dir_actual, tamanio_leer - leidos, base_ms, tamanio_ms);

        t_lectura* lectura = malloc(sizeof(t_lectura));
        lectura->direccion = (int)(dir_actual - base_ms);
        lectura->tamanio = bytes_a_leer;
        enviar_lectura(lectura, socket_ms);
        free(lectura);

        t_lectura_respuesta* respuesta = (t_lectura_respuesta*)esperar_respuesta(socket_ms);
        if (respuesta == NULL) {
            free(contenido);
            return MEM_CPU_ERROR;
        }

        if (respuesta->tamanio != bytes_a_leer) {
            eliminar_respuesta_lectura(respuesta);
            free(contenido);
            return MEM_CPU_ERROR;
        }

        memcpy(contenido + leidos, respuesta->contenido, bytes_a_leer);
        log_info(logger,
                 "PID: %d - Acción: LEER - Dirección Física: %u - Valor: %u",
                 pid_actual,
                 dir_actual,
                 valor_para_log(respuesta->contenido, bytes_a_leer));
        eliminar_respuesta_lectura(respuesta);

        leidos += bytes_a_leer;
    }

    memset(contenido + leidos, 0, 1);
    *contenido_leido = contenido;
    return MEM_CPU_OK;
}

t_resultado_memoria_cpu escribir_memoria(uint32_t dir_logica, const void* contenido, int tamanio_total) {
    uint32_t dir_fisica;
    t_resultado_memoria_cpu traduccion = traducir_rango_logico(dir_logica, tamanio_total, &dir_fisica);
    if (traduccion != MEM_CPU_OK)
        return traduccion;

    int escritos = 0;

    while (escritos < tamanio_total) {
        uint32_t dir_actual = dir_fisica + escritos;

        int socket_ms;
        uint32_t base_ms;
        int tamanio_ms;
        if (!obtener_info_ms_por_direccion_fisica(dir_actual, &socket_ms, &base_ms, &tamanio_ms)) {
            return MEM_CPU_ERROR;
        }

        int bytes_a_escribir = bytes_hasta_fin_ms(dir_actual, tamanio_total - escritos, base_ms, tamanio_ms);

        void* contenido_parcial = malloc(bytes_a_escribir);
        if (contenido_parcial == NULL)
            return MEM_CPU_ERROR;
        memcpy(contenido_parcial, contenido + escritos, bytes_a_escribir);

        t_escritura* escritura = malloc(sizeof(t_escritura));
        escritura->direccion = (int)(dir_actual - base_ms);
        escritura->tamanio = bytes_a_escribir;
        escritura->contenido = contenido_parcial;
        enviar_escritura(escritura, socket_ms);
        eliminar_escritura(escritura);
        // contenido_parcial fue liberado por eliminar_escritura (via free->contenido)

        t_escritura_respuesta* respuesta = (t_escritura_respuesta*)esperar_respuesta(socket_ms);
        if (respuesta == NULL)
            return MEM_CPU_ERROR;

        bool ok = respuesta->resultado;
        free(respuesta);
        if (!ok)
            return MEM_CPU_ERROR;

        log_info(logger,
                 "PID: %d - Acción: ESCRIBIR - Dirección Física: %u - Valor: %u",
                 pid_actual,
                 dir_actual,
                 valor_para_log(contenido + escritos, bytes_a_escribir));

        escritos += bytes_a_escribir;
    }

    return MEM_CPU_OK;
}
