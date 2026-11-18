#include "servidor.h"
#include "algoritmos.h"
#include "init.h"
#include "memory_cliente.h"
#include "planificadores.h"
#include "syscalls_kernel.h"

#include <commons/log.h>
#include <utils/comunicacion.h>
#include <utils/interrupcion.h>
#include <utils/io_utils.h>
#include <utils/syscall.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static t_list* pedidos_io_pendientes = NULL;

static char* nombre_io(op_code tipo) {
    switch (tipo) {
        case IO_STDIN:
            return "STDIN";
        case IO_STDOUT:
            return "STDOUT";
        case IO_SLEEP:
            return "SLEEP";
        default:
            return "DESCONOCIDA";
    }
}

static void asegurar_cola_io_sin_lock(void) {
    if (pedidos_io_pendientes == NULL)
        pedidos_io_pendientes = list_create();
}

static t_pedido_io* tomar_pedido_pendiente_sin_lock(op_code tipo) {
    asegurar_cola_io_sin_lock();

    for (int i = 0; i < list_size(pedidos_io_pendientes); i++) {
        t_pedido_io* pedido = list_get(pedidos_io_pendientes, i);
        if (pedido->tipo == tipo)
            return list_remove(pedidos_io_pendientes, i);
    }

    return NULL;
}

static t_io_kernel* buscar_io_libre_sin_lock(op_code tipo) {
    for (int i = 0; i < list_size(ios_conectadas); i++) {
        t_io_kernel* io = list_get(ios_conectadas, i);
        if (io->tipo == tipo && !io->ocupado)
            return io;
    }

    return NULL;
}

static void asignar_pedido_sin_lock(t_io_kernel* io, t_pedido_io* pedido) {
    io->ocupado = true;
    io->pcb = pedido->pcb;
    io->direccion = pedido->direccion;
    io->tamanio = pedido->tamanio;
    io->tiempo = pedido->tiempo;
}

static bool despachar_io_asignada(t_io_kernel* io) {
    switch (io->tipo) {
        case IO_SLEEP:
            enviar_pedido_io_sleep(io->pcb->pid, io->tiempo, io->socket);
            log_debug(logger, "## (%d) SLEEP por %d ms", io->pcb->pid, io->tiempo);
            return true;

        case IO_STDIN:
            enviar_pedido_io_stdin(io->pcb->pid, io->tamanio, io->socket);
            return true;

        case IO_STDOUT: {
            char* contenido = memoria_leer(io->pcb->pid, io->direccion, io->tamanio);
            if (contenido == NULL) {
                log_warning(logger, "STDOUT no pudo leer datos reales de Kernel Memory");
                finalizar_proceso_bloqueado_por_error(io->pcb, "SEG_FAULT");
                return false;
            }

            enviar_pedido_io_stdout(io->pcb->pid, contenido, io->socket);
            free(contenido);
            return true;
        }

        default:
            log_warning(logger, "Tipo de IO desconocido: %d", io->tipo);
            finalizar_proceso_bloqueado_por_error(io->pcb, "ERROR_IO");
            return false;
    }
}

t_io_kernel* buscar_io_por_socket(int socket) {
    pthread_mutex_lock(&mutex_ios);

    for (int i = 0; i < list_size(ios_conectadas); i++) {
        t_io_kernel* io = list_get(ios_conectadas, i);

        if (io->socket == socket) {
            pthread_mutex_unlock(&mutex_ios);
            return io;
        }
    }

    pthread_mutex_unlock(&mutex_ios);
    return NULL;
}

bool solicitar_io(op_code tipo, t_pcb* pcb, uint32_t direccion, uint32_t tamanio, int tiempo) {
    t_pedido_io* pedido = malloc(sizeof(t_pedido_io));
    if (pedido == NULL) {
        log_error(logger, "No se pudo reservar un pedido de IO para PID %d", pcb->pid);
        return false;
    }

    pedido->tipo = tipo;
    pedido->pcb = pcb;
    pedido->direccion = direccion;
    pedido->tamanio = tamanio;
    pedido->tiempo = tiempo;

    pthread_mutex_lock(&mutex_ios);
    t_io_kernel* io = buscar_io_libre_sin_lock(tipo);

    if (io == NULL) {
        asegurar_cola_io_sin_lock();
        list_add(pedidos_io_pendientes, pedido);
        pthread_mutex_unlock(&mutex_ios);

        log_debug(logger, "## (%d) Esperando IO %s disponible", pcb->pid, nombre_io(tipo));
        return true;
    }

    asignar_pedido_sin_lock(io, pedido);
    pthread_mutex_unlock(&mutex_ios);
    free(pedido);

    if (!despachar_io_asignada(io))
        liberar_io(io);

    return true;
}

void liberar_io(t_io_kernel* io) {
    if (io == NULL) {
        return;
    }

    while (1) {
        pthread_mutex_lock(&mutex_ios);
        io->ocupado = false;
        io->pcb = NULL;
        io->direccion = 0;
        io->tamanio = 0;
        io->tiempo = 0;

        t_pedido_io* siguiente = tomar_pedido_pendiente_sin_lock(io->tipo);
        if (siguiente == NULL) {
            pthread_mutex_unlock(&mutex_ios);
            return;
        }

        asignar_pedido_sin_lock(io, siguiente);
        pthread_mutex_unlock(&mutex_ios);
        free(siguiente);

        if (despachar_io_asignada(io))
            return;
    }
}

void registrar_io(op_code tipo, int socket) {
    t_io_kernel* io = malloc(sizeof(t_io_kernel));
    t_pedido_io* pendiente = NULL;

    if (io == NULL) {
        log_error(logger, "No se pudo reservar memoria para IO");
        return;
    }

    io->tipo = tipo;
    io->socket = socket;
    io->ocupado = false;
    io->pcb = NULL;
    io->direccion = 0;
    io->tamanio = 0;
    io->tiempo = 0;

    pthread_mutex_lock(&mutex_ios);
    list_add(ios_conectadas, io);
    pendiente = tomar_pedido_pendiente_sin_lock(tipo);
    if (pendiente != NULL)
        asignar_pedido_sin_lock(io, pendiente);
    pthread_mutex_unlock(&mutex_ios);

    log_info(logger, "## IO %s Conectada", nombre_io(tipo));

    if (pendiente != NULL) {
        free(pendiente);
        if (!despachar_io_asignada(io))
            liberar_io(io);
    }
}

void manejar_desconexion_io(int socket) {
    t_io_kernel* io = NULL;
    t_pedido_io* pedido_interrumpido = NULL;
    t_io_kernel* io_reemplazo = NULL;

    pthread_mutex_lock(&mutex_ios);

    for (int i = 0; i < list_size(ios_conectadas); i++) {
        t_io_kernel* candidata = list_get(ios_conectadas, i);
        if (candidata->socket != socket)
            continue;

        io = list_remove(ios_conectadas, i);
        if (io->ocupado && io->pcb != NULL) {
            pedido_interrumpido = malloc(sizeof(t_pedido_io));
            if (pedido_interrumpido != NULL) {
                pedido_interrumpido->tipo = io->tipo;
                pedido_interrumpido->pcb = io->pcb;
                pedido_interrumpido->direccion = io->direccion;
                pedido_interrumpido->tamanio = io->tamanio;
                pedido_interrumpido->tiempo = io->tiempo;

                io_reemplazo = buscar_io_libre_sin_lock(io->tipo);
                if (io_reemplazo != NULL) {
                    asignar_pedido_sin_lock(io_reemplazo, pedido_interrumpido);
                } else {
                    asegurar_cola_io_sin_lock();
                    list_add_in_index(pedidos_io_pendientes, 0, pedido_interrumpido);
                }
            }
        }
        break;
    }

    pthread_mutex_unlock(&mutex_ios);

    if (io == NULL)
        return;

    log_warning(logger, "## IO %s Desconectada", nombre_io(io->tipo));

    if (io->ocupado && io->pcb != NULL && pedido_interrumpido == NULL)
        finalizar_proceso_bloqueado_por_error(io->pcb, "ERROR_IO");

    free(io);

    if (io_reemplazo != NULL) {
        free(pedido_interrumpido);
        if (!despachar_io_asignada(io_reemplazo))
            liberar_io(io_reemplazo);
    }
}

static void atender_respuesta_stdin(int socket_io) {
    t_io_kernel* io = buscar_io_por_socket(socket_io);
    char* contenido = recibir_respuesta_io_stdin(socket_io);

    if (io == NULL || io->pcb == NULL) {
        log_warning(logger, "Respuesta STDIN recibida sin proceso asociado");
        free(contenido);
        return;
    }

    if (io->pcb->estado == SUSP_BLOCK && !memoria_desuspender_proceso(io->pcb->pid)) {
        log_warning(logger, "STDIN no pudo restaurar PID %d desde SWAP", io->pcb->pid);
        finalizar_proceso_bloqueado_por_error(io->pcb, "ERROR_MEMORIA");
        liberar_io(io);
        free(contenido);
        return;
    }

    bool ok = memoria_escribir(io->pcb->pid, io->direccion, io->tamanio, contenido);

    if (!ok && io->pcb->estado == SUSP_BLOCK) {
        if (memoria_desuspender_proceso(io->pcb->pid))
            ok = memoria_escribir(io->pcb->pid, io->direccion, io->tamanio, contenido);
    }

    if (!ok) {
        log_warning(logger, "STDIN no pudo escribir en Kernel Memory para PID %d", io->pcb->pid);
        finalizar_proceso_bloqueado_por_error(io->pcb, "SEG_FAULT");
        liberar_io(io);
        free(contenido);
        return;
    }

    completar_io_de_proceso(io->pcb);
    liberar_io(io);
    free(contenido);
}

static void atender_respuesta_stdout(int socket_io) {
    t_io_kernel* io = buscar_io_por_socket(socket_io);
    bool ok = recibir_respuesta_io_generica(socket_io);

    if (io == NULL || io->pcb == NULL) {
        log_warning(logger, "Respuesta STDOUT recibida sin proceso asociado");
        return;
    }

    if (!ok) {
        log_warning(logger, "IO STDOUT informó error para PID %d", io->pcb->pid);
    }

    completar_io_de_proceso(io->pcb);
    liberar_io(io);
}

static void atender_respuesta_sleep(int socket_io) {
    t_io_kernel* io = buscar_io_por_socket(socket_io);
    bool ok = recibir_respuesta_io_generica(socket_io);

    if (io == NULL || io->pcb == NULL) {
        log_warning(logger, "Respuesta SLEEP recibida sin proceso asociado");
        return;
    }

    if (!ok) {
        log_warning(logger, "IO SLEEP informó error para PID %d", io->pcb->pid);
    }

    completar_io_de_proceso(io->pcb);
    liberar_io(io);
}

// =========================
// HILO POR CLIENTE
//==================================================
void* atender_cliente(void* arg) {

    int cliente_fd = *(int *) arg;
    bool es_cpu = false;
    bool es_io = false;
    free(arg);

    while (1) {

        int op = recibir_operacion(cliente_fd);

        if (op == -1) {
            if (es_cpu)
                manejar_desconexion_cpu(cliente_fd);
            else if (es_io)
                manejar_desconexion_io(cliente_fd);
            else
                log_debug(logger, "Cliente desconectado");
            break;
        }

        switch (op) {

            case MENSAJE_CPU: {
                int id_cpu = recibir_operacion(cliente_fd);
                es_cpu = true;
                registrar_cpu_planificador_con_id(id_cpu, cliente_fd, cliente_fd);
                break;
            }

            case MENSAJE_IO: {
                op_code tipo_io = recibir_operacion(cliente_fd);
                es_io = true;
                registrar_io(tipo_io, cliente_fd);
                break;
            }

            case MENSAJE: {
                int size;
                char* buffer = recibir_buffer(&size, cliente_fd);
                log_debug(logger, "Mensaje: %s", buffer);
                free(buffer);
                break;
            }

            case PAQUETE: {
                log_debug(logger, "Paquete recibido (pendiente implementación)");
                break;
            }

            case SYSCALL: {
                t_syscall* syscall = recibir_t_syscall(cliente_fd);
                procesar_syscall_kernel(cliente_fd, syscall);
                eliminar_t_syscall(syscall);
                break;
            }

            case CPU_DESALOJADA: {
                int pid = recibir_cpu_desalojada(cliente_fd);

                procesar_desalojo_cpu(cliente_fd, pid);
                break;
            }

            case CPU_ERROR: {
                int pid = recibir_error_cpu(cliente_fd);
                t_pcb* pcb = sacar_pcb_de_exec_por_pid(pid);

                liberar_cpu_por_socket(cliente_fd);

                if (pcb == NULL) {
                    log_warning(logger,
                                "CPU informó error para PID %d, pero no estaba en EXEC",
                                pid);
                    break;
                }

                pasar_a_exit_con_motivo(pcb, "ERROR_MEMORIA");
                break;
            }

            case CPU_SEG_FAULT: {
                int pid = recibir_error_cpu(cliente_fd);
                t_pcb* pcb = sacar_pcb_de_exec_por_pid(pid);

                liberar_cpu_por_socket(cliente_fd);

                if (pcb == NULL) {
                    log_warning(logger,
                                "CPU informó SEG_FAULT para PID %d, pero no estaba en EXEC",
                                pid);
                    break;
                }

                pasar_a_exit_con_motivo(pcb, "SEG_FAULT");
                break;
            }

            case IO_STDIN: {
                atender_respuesta_stdin(cliente_fd);
                break;
            }

            case IO_STDOUT: {
                atender_respuesta_stdout(cliente_fd);
                break;
            }

            case IO_SLEEP: {
                atender_respuesta_sleep(cliente_fd);
                break;
            }
            
// TODO: Activar cuando Kernel Memory implemente detección de corrupción
            case BSOD: { //Recibe de MEMORY pARA FINALIZAR 
                log_error(logger, "## Se detecto corrupcion de memoria - BSOD");
                finalizar_todos_los_procesos();
                log_error(logger, "## Kernel Scheduler finalizado por BSOD");
                sleep(1);
                exit(EXIT_FAILURE);
            }

            default:
                log_warning(logger, "Operacion desconocida: %d", op);
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}
