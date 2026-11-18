#include "planificadores.h"
#include "algoritmos.h"
#include "config.h"
#include "init.h"
#include "memory_cliente.h"
#include "mutex.h"

#include <commons/log.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static t_algoritmo_planificacion parsear_algoritmo_planificacion(char* valor) {
    if (valor == NULL) {
        return ALGORITMO_FIFO;
    }

    if (strcasecmp(valor, "FIFO") == 0) {
        return ALGORITMO_FIFO;
    }

    if (strcasecmp(valor, "RR") == 0) {
        return ALGORITMO_RR;
    }

    if (strcasecmp(valor, "CMN") == 0) {
        return ALGORITMO_CMN;
    }

    log_warning(logger, "Algoritmo de planificacion desconocido '%s', se usa FIFO", valor);
    return ALGORITMO_FIFO;
}

void inicializar_algoritmos_planificacion(void) {
    algoritmo_largo_plazo = ALGORITMO_FIFO;
    algoritmo_corto_plazo = parsear_algoritmo_planificacion(PLANIFICATION_ALGORITHM);
}

static bool memoria_acepta_proceso(t_pcb* pcb) {
    (void)pcb;
    return true;
}

static t_pcb* seleccionar_siguiente_new(void) {
    if (list_is_empty(cola_new)) {
        return NULL;
    }

    return list_remove(cola_new, 0);
}

static void reencolar_en_new(t_pcb* pcb) {
    list_add_in_index(cola_new, 0, pcb);
}

static void liberar_timer_block_por_pcb(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_block_timers);

    for (int i = 0; i < list_size(lista_block_timers); i++) {
        t_block_entry* entry = list_get(lista_block_timers, i);

        if (entry->pcb == pcb) {
            list_remove(lista_block_timers, i);
            free(entry);
            break;
        }
    }

    pthread_mutex_unlock(&mutex_block_timers);
}

static void vaciar_lista_bsod(t_list* lista) {
    while (!list_is_empty(lista)) {
        t_pcb* pcb = list_remove(lista, 0);

        log_info(logger, "## (%d) finalizó su ejecución con motivo de BSOD", pcb->pid);

        // BUG 2: Liberar mutexes antes de notificar a memoria
        liberar_mutexes_por_proceso(pcb);

        // BUG 3: Notificar a kernel_memory para liberar recursos
        memoria_finalizar_proceso(pcb->pid);

        // Limpiar timers asociados a este proceso
        liberar_timer_block_por_pcb(pcb);

        free(pcb);
    }
}

void finalizar_todos_los_procesos(void) {
    // BUG 3: Lockear los 7 mutexes (no solo 4)
    pthread_mutex_lock(&mutex_new);
    pthread_mutex_lock(&mutex_ready);
    pthread_mutex_lock(&mutex_exec);
    pthread_mutex_lock(&mutex_block);
    pthread_mutex_lock(&mutex_exit);
    pthread_mutex_lock(&mutex_susp_block);
    pthread_mutex_lock(&mutex_susp_ready);

    vaciar_lista_bsod(cola_new);
    vaciar_lista_bsod(cola_ready);
    vaciar_lista_bsod(cola_exec);
    vaciar_lista_bsod(cola_block);
    vaciar_lista_bsod(cola_exit);
    vaciar_lista_bsod(cola_susp_block);
    vaciar_lista_bsod(cola_susp_ready);

    pthread_mutex_unlock(&mutex_susp_ready);
    pthread_mutex_unlock(&mutex_susp_block);
    pthread_mutex_unlock(&mutex_exit);
    pthread_mutex_unlock(&mutex_block);
    pthread_mutex_unlock(&mutex_exec);
    pthread_mutex_unlock(&mutex_ready);
    pthread_mutex_unlock(&mutex_new);
}

bool comparar_prioridad_y_tiempo(void* a, void* b) {
    t_pcb* pcb1 = (t_pcb*) a;
    t_pcb* pcb2 = (t_pcb*) b;

    if (pcb1->prioridad < pcb2->prioridad) return true;
    if (pcb1->prioridad > pcb2->prioridad) return false;

    return pcb1->tiempo_suspendido > pcb2->tiempo_suspendido;
}

void liberar_memoria(t_pcb* pcb) {
    if (!memoria_finalizar_proceso(pcb->pid)) {
        log_error(logger, "Error liberando memoria de PID %d", pcb->pid);
    }
}

void pasar_a_exit_con_motivo(t_pcb* pcb, const char* motivo) {
    if (pcb == NULL) {
        return;
    }

    // BUG 2: Liberar todos los mutexes que posee o en los que está bloqueado
    liberar_mutexes_por_proceso(pcb);

    t_estado estado_anterior = pcb->estado;
    pcb->estado = EXIT;

    pthread_mutex_lock(&mutex_exit);
    list_add(cola_exit, pcb);
    pthread_mutex_unlock(&mutex_exit);

    loggear_cambio_estado(pcb, estado_anterior, EXIT);
    log_info(logger, "## (%d) finalizó su ejecución con motivo de %s", pcb->pid, motivo);

    liberar_memoria(pcb);
}

void pasar_a_exit(t_pcb* pcb) {
    pasar_a_exit_con_motivo(pcb, "EXIT");
}


//FUNCIONES DE PLANIFICACION 
// =========================

// PLANIFICADOR LARGO PLAZO = NEW a READY
// ======================================
void* planificador_largo_plazo(void* arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&mutex_new);
        t_pcb* pcb = seleccionar_siguiente_new();
        pthread_mutex_unlock(&mutex_new);

        if (pcb == NULL) {
            usleep(500000);
            continue;
        }

        if (!memoria_acepta_proceso(pcb)) {
            pthread_mutex_lock(&mutex_new);
            reencolar_en_new(pcb);
            pthread_mutex_unlock(&mutex_new);

            usleep(500000);
            continue;
        }

        agregar_a_ready(pcb, NEW);

        usleep(500000);
    }
}




// PLANIFICADOR CORTO PLAZO = READY A EXEC (LO BASICO)
// ==================================================

// PLANIFICADOR MEDIANO PLAZO
// =========================
void* planificador_mediano_plazo(void* arg) {

    while (1) {

        // ====================================
        // 1. BLOCK → SUSP_BLOCK (por timeout)
        // ====================================
        pthread_mutex_lock(&mutex_block_timers);

        for (int i = 0; i < list_size(lista_block_timers); ) {

            t_block_entry* entry = list_get(lista_block_timers, i);

            time_t ahora = time(NULL);
            int tiempo = (int) difftime(ahora, entry->inicio_block) * 1000;

            if (tiempo < SUSPENSION_TIMEOUT) {
                i++;
                continue;
            }

            pthread_mutex_lock(&mutex_block);

            if (list_remove_element(cola_block, entry->pcb)) {

                pthread_mutex_unlock(&mutex_block);

                if (!memoria_suspender_proceso(entry->pcb->pid)) {
                    log_warning(logger, "## (%d) No se pudo suspender en Kernel Memory", entry->pcb->pid);
                    pthread_mutex_lock(&mutex_block);
                    list_add(cola_block, entry->pcb);
                    pthread_mutex_unlock(&mutex_block);
                    i++;
                    continue;
                }

                t_estado estado_anterior = entry->pcb->estado;
                entry->pcb->estado = SUSP_BLOCK;
                entry->pcb->tiempo_suspendido = time(NULL);

                pthread_mutex_lock(&mutex_susp_block);
                list_add(cola_susp_block, entry->pcb);
                pthread_mutex_unlock(&mutex_susp_block);

                loggear_cambio_estado(entry->pcb, estado_anterior, SUSP_BLOCK);

            } else {
                pthread_mutex_unlock(&mutex_block);
            }

            list_remove_element(lista_block_timers, entry);
            free(entry);
        }

        pthread_mutex_unlock(&mutex_block_timers);


        // =========================================================
        // 2. SUSP_READY → READY
        // =========================================================
        if (memoria_ampliada_pendiente) {
            memoria_ampliada_pendiente = false;
            manejar_ampliacion_memoria();
        }

        pthread_mutex_lock(&mutex_susp_ready);

        list_sort(cola_susp_ready, comparar_prioridad_y_tiempo);

        for (int i = 0; i < list_size(cola_susp_ready); ) {

            t_pcb* pcb = list_get(cola_susp_ready, i);

            bool hay_memoria = memoria_desuspender_proceso(pcb->pid);

            if (!hay_memoria) {
                pcb->intentos_memoria++;
                log_warning(logger, "## (%d) Memoria rechazada (%d intentos)", pcb->pid, pcb->intentos_memoria);

                if (pcb->intentos_memoria >= 3) {
                    list_remove_element(cola_susp_ready, pcb);
                    pasar_a_exit_con_motivo(pcb, "SIN_MEMORIA");
                    continue;
                }

                i++;
                continue;
            }

            list_remove_element(cola_susp_ready, pcb);
            pcb->intentos_memoria = 0;
            agregar_a_ready(pcb, SUSP_READY);

            // no incrementas i porque removiste elemento
        }

        pthread_mutex_unlock(&mutex_susp_ready);

        usleep(100000);
    }

    return NULL;
}


// MANEJO DE ERROR GLOBAL (BSOD), PROVISORIAMENTE HASTA VER SI SE PUEDE CAMBIAR
// TODO: Activar cuando Kernel Memory implemente detección de corrupción

/* ESTE NO ES SIMULADOR SI NO UN CODIGO EN BUEN ESTADO , SOLO QUE ANTES ES PREFERIBLE QUE HAGA EL PLANIFICADOR DE CORTO PLAZO 
// =========================
// TIMER DE SUSPENSION (MEDIANO PLAZO) ACTIVALO CUNDO EL PLANIFICADOR DE CORTO PLAZO ESTE LISTO 
// =========================
void* timer_suspension(void* arg) {

    t_pcb* pcb = (t_pcb*) arg;

    usleep(SUSPENSION_TIMEOUT * 1000); // viene del config

    pthread_mutex_lock(&mutex_block);

    bool sigue_en_block = list_contains(cola_block, pcb);

    if (sigue_en_block) {

        list_remove_element(cola_block, pcb);
        pthread_mutex_unlock(&mutex_block);

        pcb->estado = SUSP_BLOCK;

        pthread_mutex_lock(&mutex_block); 
        list_add(cola_susp_block, pcb);
        pthread_mutex_unlock(&mutex_block);

        log_info(logger, "## (%d) Pasa del estado BLOCK al estado SUSP. BLOCK", pcb->pid);

    } else {
        pthread_mutex_unlock(&mutex_block);
    }

    return NULL;
}
*/
