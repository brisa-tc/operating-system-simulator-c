#include "syscalls_kernel.h"
#include "algoritmos.h"
#include "init.h"
#include "memory_cliente.h"
#include "mutex.h"
#include "planificadores.h"
#include "servidor.h"

#include <commons/collections/list.h>
#include <utils/io_utils.h>
#include <unistd.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//static ya que no deverian ser llamadas desde otros modulos , asi que lo puse asi por las dudas
static t_pcb* obtener_pcb_por_socket_cpu(int socket_cpu) {
    t_pcb* pcb = NULL;

    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->socket_dispatch == socket_cpu || cpu->socket_interrupt == socket_cpu) {
            pcb = cpu->pcb;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);

    return pcb;
}

static void sacar_de_exec_y_liberar_cpu(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_exec);
    list_remove_element(cola_exec, pcb);
    pthread_mutex_unlock(&mutex_exec);

    liberar_cpu_por_pid(pcb->pid);
}

static void bloquear_por_syscall(t_pcb* pcb) {
    sacar_de_exec_y_liberar_cpu(pcb);

    cambiar_estado(pcb, BLOCK);

    pthread_mutex_lock(&mutex_block);
    list_add(cola_block, pcb);
    pthread_mutex_unlock(&mutex_block);
}

static void devolver_a_ready(t_pcb* pcb) {
    sacar_de_exec_y_liberar_cpu(pcb);
    agregar_a_ready(pcb, EXEC);
}

static void registrar_timer_block(t_pcb* pcb) {
    t_block_entry* entry = malloc(sizeof(t_block_entry));
    if (entry == NULL) {
        log_error(logger, "No se pudo reservar timer de BLOCK");
        return;
    }

    entry->pcb = pcb;
    entry->inicio_block = time(NULL);

    pthread_mutex_lock(&mutex_block_timers);
    list_add(lista_block_timers, entry);
    pthread_mutex_unlock(&mutex_block_timers);
}

static void quitar_timer_block(t_pcb* pcb) {
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

static bool sacar_de_block(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_block);
    bool removido = list_remove_element(cola_block, pcb);
    pthread_mutex_unlock(&mutex_block);

    return removido;
}

static bool sacar_de_susp_block(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_susp_block);
    bool removido = list_remove_element(cola_susp_block, pcb);
    pthread_mutex_unlock(&mutex_susp_block);

    return removido;
}

static void finalizar_io_a_susp_ready(t_pcb* pcb) {
    t_estado estado_anterior = pcb->estado;
    pcb->estado = SUSP_READY;

    pthread_mutex_lock(&mutex_susp_ready);
    list_add(cola_susp_ready, pcb);
    pthread_mutex_unlock(&mutex_susp_ready);

    log_info(logger, "## (%d) finalizó IO y pasa a SUSP. READY", pcb->pid);
    loggear_cambio_estado(pcb, estado_anterior, SUSP_READY);
}

void completar_io_de_proceso(t_pcb* pcb) {
    if (pcb == NULL) {
        return;
    }

    quitar_timer_block(pcb);

    if (sacar_de_block(pcb)) {
        log_info(logger, "## (%d) finalizó IO y pasa a READY", pcb->pid);
        agregar_a_ready(pcb, BLOCK);
        return;
    }

    if (sacar_de_susp_block(pcb)) {
        finalizar_io_a_susp_ready(pcb);
        return;
    }
}

void finalizar_proceso_bloqueado_por_error(t_pcb* pcb, const char* motivo) {
    if (pcb == NULL) {
        return;
    }

    quitar_timer_block(pcb);
    sacar_de_block(pcb);
    sacar_de_susp_block(pcb);
    pasar_a_exit_con_motivo(pcb, motivo);
}

static void crear_proceso_desde_syscall(char* path, int prioridad) {
    static int proximo_pid = 1;

    int pid = proximo_pid++;

    if (!memoria_crear_proceso(pid, path)) {
        log_error(logger, "No se pudo crear proceso PID %d", pid);
        return;
    }

    t_pcb* pcb = malloc(sizeof(t_pcb));
    if (pcb == NULL) {
        log_error(logger, "No se pudo reservar PCB para PID %d", pid);
        return;
    }

    pcb->pid = pid;
    pcb->estado = NEW;
    pcb->prioridad = prioridad;
    pcb->prioridad_original = prioridad;
    pcb->tiempo_suspendido = 0;
    pcb->intentos_memoria = 0;

    pthread_mutex_lock(&mutex_new);
    list_add(cola_new, pcb);
    pthread_mutex_unlock(&mutex_new);

    log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", pid);
}


static void set_compactacion_en_progreso(bool valor) {
    pthread_mutex_lock(&mutex_compactacion);
    compactacion_en_progreso = valor;
    pthread_mutex_unlock(&mutex_compactacion);
}

static bool hay_procesos_en_exec(void) {
    pthread_mutex_lock(&mutex_exec);
    bool hay = !list_is_empty(cola_exec);
    pthread_mutex_unlock(&mutex_exec);

    return hay;
}

static void interrumpir_cpus_por_compactacion(void) {
    t_list* cpus_a_interrumpir = list_create();

    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (!cpu->libre && cpu->pcb != NULL) {
            list_add(cpus_a_interrumpir, cpu);
        }
    }

    pthread_mutex_unlock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_a_interrumpir); i++) {
        t_cpu_kernel* cpu = list_get(cpus_a_interrumpir, i);
        enviar_interrupcion_a_cpu(cpu, MOTIVO_COMPACTACION);
    }

    list_destroy(cpus_a_interrumpir);
}

static void esperar_cpus_desalojadas_por_compactacion(void) {
    while (hay_procesos_en_exec()) {
        usleep(100000);
    }
}

static op_code resolver_compactacion_para_mem_alloc(t_pcb* pcb, t_sc_mem_alloc* datos) {
    set_compactacion_en_progreso(true);

    sacar_de_exec_y_liberar_cpu(pcb);

    log_info(logger, "## Inicio de compactación");

    interrumpir_cpus_por_compactacion();
    esperar_cpus_desalojadas_por_compactacion();

    if (!memoria_solicitar_compactacion()) {
        set_compactacion_en_progreso(false);
        return MEM_ERROR;
    }

    log_info(logger, "## Fin de compactación");

    op_code respuesta = memoria_crear_segmento(
        pcb->pid,
        datos->id_segmento,
        datos->tamanio
    );

    set_compactacion_en_progreso(false);

    return respuesta;
}


void procesar_syscall_kernel(int socket_cpu, t_syscall* syscall) {
    t_pcb* pcb = obtener_pcb_por_socket_cpu(socket_cpu);

    if (pcb == NULL) {
        log_warning(logger, "Syscall recibida sin PCB asociado a CPU");
        return;
    }

    switch (syscall->tipo_syscall) {
        case SC_SLEEP: {
            log_info(logger, "## (%d) - Solicitó syscall: SLEEP", pcb->pid);

            t_sc_sleep* datos = deserializar_sc_sleep(syscall);

            bloquear_por_syscall(pcb);
            registrar_timer_block(pcb);

            if (!solicitar_io(IO_SLEEP, pcb, 0, 0, datos->tiempo)) {
                finalizar_proceso_bloqueado_por_error(pcb, "ERROR_IO");
            }

            free(datos);
            break;
        }

        case SC_INIT_PROC: {
            log_info(logger, "## (%d) - Solicitó syscall: INIT_PROC", pcb->pid);

            t_sc_init* datos = deserializar_sc_init(syscall);

            crear_proceso_desde_syscall(datos->path, datos->prioridad);
            devolver_a_ready(pcb);

            free(datos->path);
            free(datos);
            break;
        }

        case SC_MUTEX_CREATE: {
            log_info(logger, "## (%d) - Solicitó syscall: MUTEX_CREATE", pcb->pid);

            t_sc_mutex* datos = deserializar_sc_mutex(syscall);

            mutex_create(datos->nombre_mutex);
            devolver_a_ready(pcb);

            free(datos->nombre_mutex);
            free(datos);
            break;
        }

        case SC_MUTEX_LOCK: {
            log_info(logger, "## (%d) - Solicitó syscall: MUTEX_LOCK", pcb->pid);

            t_sc_mutex* datos = deserializar_sc_mutex(syscall);

            sacar_de_exec_y_liberar_cpu(pcb);
            bool continua = mutex_lock(pcb, datos->nombre_mutex);

            if (continua) {
                agregar_a_ready(pcb, EXEC);
            }

            free(datos->nombre_mutex);
            free(datos);
            break;
        }

        case SC_MUTEX_UNLOCK: {
            log_info(logger, "## (%d) - Solicitó syscall: MUTEX_UNLOCK", pcb->pid);

            t_sc_mutex* datos = deserializar_sc_mutex(syscall);

            mutex_unlock(pcb, datos->nombre_mutex);
            devolver_a_ready(pcb);

            free(datos->nombre_mutex);
            free(datos);
            break;
        }

        case SC_EXIT: {
            log_info(logger, "## (%d) - Solicitó syscall: EXIT", pcb->pid);

            sacar_de_exec_y_liberar_cpu(pcb);
            pasar_a_exit_con_motivo(pcb, "EXIT");
            break;
        }

        case SC_MEM_ALLOC: {
            log_info(logger, "## (%d) - Solicitó syscall: MEM_ALLOC", pcb->pid);

            t_sc_mem_alloc* datos = deserializar_sc_mem_alloc(syscall);

            op_code respuesta = memoria_crear_segmento(
                pcb->pid,
                datos->id_segmento,
                datos->tamanio
            );

            if (respuesta == MEM_OK) {
                devolver_a_ready(pcb);

            } else if (respuesta == MEM_COMPACTACION) {
                op_code respuesta_post_compactacion =
                    resolver_compactacion_para_mem_alloc(pcb, datos);

                if (respuesta_post_compactacion == MEM_OK) {
                    agregar_a_ready(pcb, EXEC);
                } else {
                    pasar_a_exit_con_motivo(pcb, "ERROR_MEMORIA");
                }

            } else if (respuesta == MEM_ERROR || respuesta == MEM_FULL) {
                sacar_de_exec_y_liberar_cpu(pcb);
                pasar_a_exit_con_motivo(pcb, "ERROR_MEMORIA");

            } else {
                log_warning(logger, "Respuesta inesperada de Kernel Memory: %d", respuesta);

                sacar_de_exec_y_liberar_cpu(pcb);
                pasar_a_exit_con_motivo(pcb, "ERROR_MEMORIA");
            }

            free(datos);
            break;
        }

        case SC_MEM_FREE: {
            log_info(logger, "## (%d) - Solicitó syscall: MEM_FREE", pcb->pid);

            t_sc_mem_free* datos = deserializar_sc_mem_free(syscall);

            bool ok = memoria_eliminar_segmento(
                pcb->pid,
                datos->id_segmento
            );

            if (ok) {
                devolver_a_ready(pcb);
            } else {
                sacar_de_exec_y_liberar_cpu(pcb);
                pasar_a_exit_con_motivo(pcb, "ERROR_MEMORIA");
            }

            free(datos);
            break;
        }

        case SC_STDIN: {
            log_info(logger, "## (%d) - Solicitó syscall: STDIN", pcb->pid);
            t_sc_io* datos_stdin = deserializar_sc_io(syscall);

            bloquear_por_syscall(pcb);
            registrar_timer_block(pcb);

            if (!solicitar_io(
                    IO_STDIN,
                    pcb,
                    datos_stdin->direccion,
                    datos_stdin->tamanio,
                    0)) {
                finalizar_proceso_bloqueado_por_error(pcb, "ERROR_IO");
            }

            free(datos_stdin);
            break;
        }

        case SC_STDOUT: {
            log_info(logger, "## (%d) - Solicitó syscall: STDOUT", pcb->pid);
            t_sc_io* datos_stdout = deserializar_sc_io(syscall);

            bloquear_por_syscall(pcb);
            registrar_timer_block(pcb);

            if (!solicitar_io(
                    IO_STDOUT,
                    pcb,
                    datos_stdout->direccion,
                    datos_stdout->tamanio,
                    0)) {
                finalizar_proceso_bloqueado_por_error(pcb, "ERROR_IO");
            }

            free(datos_stdout);
            break;
        }

        default:
            log_warning(logger, "Syscall desconocida: %d", syscall->tipo_syscall);
            break;
    }
}
