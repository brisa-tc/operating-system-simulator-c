#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <utils/comunicacion.h>
#include <utils/identificadores.h>
#include <utils/instruccion.h>
#include <utils/memorySticks.h>
#include <utils/contexto_utils.h>
#include <utils/swapInfo.h>
#include <pthread.h>
#include "estructura.h"
#include "procesos.h"
#include "instrucciones.h"
#include "segmentos.h"
#include "handlers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

extern t_log*    logger;
extern t_config* config;
extern char*     puerto_escucha;
extern char*     scripts_basepath;
extern int       instruction_delay;
extern int       compaction_delay;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static char* construir_path_instrucciones(char* path_relativo)
{
    if (path_relativo == NULL || path_relativo[0] == '\0')
        return NULL;

    if (path_relativo[0] == '/') {
        if (access(path_relativo, R_OK) != 0)
            return NULL;
        return strdup(path_relativo);
    }

    // Mantiene compatibilidad con rutas relativas que ya son validas desde
    // el directorio de ejecucion de Kernel Memory.
    if (access(path_relativo, R_OK) == 0)
        return strdup(path_relativo);

    if (scripts_basepath == NULL || scripts_basepath[0] == '\0')
        return NULL;

    int necesita_barra = scripts_basepath[strlen(scripts_basepath) - 1] != '/';
    int tamanio = strlen(scripts_basepath) + necesita_barra + strlen(path_relativo) + 1;
    char* path_completo = malloc(tamanio);

    if (path_completo == NULL)
        return NULL;

    snprintf(
        path_completo,
        tamanio,
        "%s%s%s",
        scripts_basepath,
        necesita_barra ? "/" : "",
        path_relativo
    );

    if (access(path_completo, R_OK) != 0) {
        free(path_completo);
        return NULL;
    }

    return path_completo;
}

static t_cpu_conectada* buscar_cpu_por_socket(int socket_cpu)
{
    pthread_mutex_lock(&mutex_cpus);

    t_cpu_conectada* resultado = NULL;
    for (int i = 0; i < list_size(lista_cpus); i++) {
        t_cpu_conectada* cpu = list_get(lista_cpus, i);
        if (cpu->socket == socket_cpu) {
            resultado = cpu;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
    return resultado;
}

static void eliminar_cpu_por_socket(int socket_cpu)
{
    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(lista_cpus); i++) {
        t_cpu_conectada* cpu = list_get(lista_cpus, i);
        if (cpu->socket == socket_cpu) {
            list_remove(lista_cpus, i);
            log_warning(logger, "## CPU %d Desconectada", cpu->id);
            free(cpu);
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
}

static void notificar_bsod_por_memory_stick(int socket_memory_stick)
{
    static pthread_mutex_t mutex_bsod = PTHREAD_MUTEX_INITIALIZER;
    static bool bsod_notificado = false;

    pthread_mutex_lock(&mutex_bsod);
    if (bsod_notificado) {
        pthread_mutex_unlock(&mutex_bsod);
        return;
    }

    bsod_notificado = true;
    pthread_mutex_unlock(&mutex_bsod);

    log_error(logger, "## Memory Stick Desconectada - memoria corrupta");

    pthread_mutex_lock(&mutex_memory_sticks);
    for (int i = 0; i < list_size(lista_memory_sticks); i++) {
        t_memoryStick_info* ms = list_get(lista_memory_sticks, i);
        if (ms->socket == socket_memory_stick) {
            list_remove(lista_memory_sticks, i);
            free(ms->ip);
            free(ms);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_memory_sticks);

    pthread_mutex_lock(&mutex_cpus);
    for (int i = 0; i < list_size(lista_cpus); i++) {
        t_cpu_conectada* cpu = list_get(lista_cpus, i);
        pthread_mutex_lock(&mutex_memory_sticks);
        enviar_lista_memory_sticks(lista_memory_sticks, cpu->socket);
        pthread_mutex_unlock(&mutex_memory_sticks);
    }
    pthread_mutex_unlock(&mutex_cpus);

    if (socket_kernel_scheduler != -1) {
        enviar_codigo(BSOD, socket_kernel_scheduler);
    } else {
        log_warning(logger, "No hay Kernel Scheduler conectado para notificar BSOD");
    }

    close(socket_memory_stick);
}

static void esperar_desconexion_memory_stick(int socket_memory_stick)
{
    while (1) {
        int op;
        int bytes = recv(socket_memory_stick, &op, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

        if (bytes == 0) {
            notificar_bsod_por_memory_stick(socket_memory_stick);
            return;
        }

        if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            notificar_bsod_por_memory_stick(socket_memory_stick);
            return;
        }

        sleep(1);
    }
}

// ─── Hilo de atención de cliente ──────────────────────────────────────────────

void* manejar_cliente(void* arg)
{
    int socket_cliente = *(int*)arg;
    bool es_cpu = false;
    free(arg);

    while (1)
    {
        int cod_op = recibir_operacion(socket_cliente);

        if (cod_op == -1) {
            if (es_cpu)
                eliminar_cpu_por_socket(socket_cliente);
            else if (socket_cliente == socket_kernel_scheduler) {
                socket_kernel_scheduler = -1;
                log_warning(logger, "## Kernel Scheduler Desconectado (FD: %d)", socket_cliente);
            } else
                log_warning(logger, "Cliente desconectado (FD: %d)", socket_cliente);
            break;
        }

        switch (cod_op)
        {
            // ─── Identificación ───────────────────────────────────────────

            case MENSAJE_CPU: {
                int id_cpu = recibir_operacion(socket_cliente);
                es_cpu = true;
                t_cpu_conectada* cpu = malloc(sizeof(t_cpu_conectada));
                if (cpu == NULL) {
                    log_error(logger, "No se pudo reservar memoria para CPU %d", id_cpu);
                    break;
                }

                cpu->id = id_cpu;
                cpu->socket = socket_cliente;
                cpu->pid_actual = -1;

                pthread_mutex_lock(&mutex_cpus);
                list_add(lista_cpus, cpu);
                pthread_mutex_unlock(&mutex_cpus);

                log_info(logger, "## CPU %d Conectada", id_cpu);
                pthread_mutex_lock(&mutex_memory_sticks);
                enviar_lista_memory_sticks(lista_memory_sticks, socket_cliente);
                pthread_mutex_unlock(&mutex_memory_sticks);
                enviar_segment_max_size(segment_max_size, socket_cliente);
                break;
            }

            case MENSAJE_SCHEDULER:
                socket_kernel_scheduler = socket_cliente;
                log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", socket_cliente);
                break;

            case MENSAJE_SWAP: {
                int op_info = recibir_operacion(socket_cliente);
                if (op_info != SWAP_BLOCK) {
                    log_warning(logger, "SWAP envio operacion inesperada: %d", op_info);
                    break;
                }

                socket_swap = socket_cliente;
                recibir_swap_block_info(socket_cliente, &swap_block_size, &swap_file_size);
                inicializar_bitmap_swap(swap_block_size, swap_file_size);

                log_info(logger,
                    "## SWAP Conectado - FD del socket: %d - Block Size: %d - Swap Size: %d",
                    socket_cliente,
                    swap_block_size,
                    swap_file_size
                );

                while (1) {
                    int op;
                    int bytes = recv(socket_cliente, &op, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

                    if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        log_warning(logger, "## SWAP Desconectado (FD: %d)", socket_cliente);
                        socket_swap = -1;
                        swap_block_size = 0;
                        swap_file_size = 0;
                        break;
                    }
                    sleep(1);
                }
                return NULL;
            }

            case MENSAJE_MEMORY_STICK: {
                int op_info = recibir_operacion(socket_cliente);
                if (op_info != MEMORY_STICKS) {
                    log_warning(logger, "Memory Stick envio operacion inesperada: %d", op_info);
                    break;
                }

                t_memoryStick_info* info = recibir_memory_stick_info(socket_cliente);

                info->socket = socket_cliente;
                info->base   = ampliar_memoria_usuario(info->tamanio);
                if (info->ip == NULL || strlen(info->ip) == 0) {
                    free(info->ip);
                    info->ip = obtener_ip_cliente(socket_cliente);
                }

                pthread_mutex_lock(&mutex_memory_sticks);
                list_add(lista_memory_sticks, info);
                pthread_mutex_unlock(&mutex_memory_sticks);

    log_info(logger, "## Memory Stick de %d bytes Conectada", info->tamanio);

    if (socket_kernel_scheduler != -1)
        enviar_codigo(MEM_AMPLIADA, socket_kernel_scheduler);

    pthread_mutex_lock(&mutex_cpus);
    for (int i = 0; i < list_size(lista_cpus); i++) {
        t_cpu_conectada* cpu = list_get(lista_cpus, i);
        pthread_mutex_lock(&mutex_memory_sticks);
        enviar_lista_memory_sticks(lista_memory_sticks, cpu->socket);
        pthread_mutex_unlock(&mutex_memory_sticks);
    }
    pthread_mutex_unlock(&mutex_cpus);

    esperar_desconexion_memory_stick(socket_cliente);
                return NULL;
            }

            // ─── Operaciones reales ───────────────────────────────────────

            case CREATE_PROCESS: {
                t_list* valores = recibir_paquete(socket_cliente);

                int   pid           = *(int*)list_get(valores, 0);
                char* path_relativo = list_get(valores, 1);
                char* path_completo = construir_path_instrucciones(path_relativo);

                if (path_completo != NULL && crear_proceso(pid, path_completo)) {
                    enviar_codigo(MEM_OK, socket_cliente);
                } else {
                    log_warning(logger,
                                "## PID: %d - No se pudo resolver el path de instrucciones: %s",
                                pid,
                                path_relativo != NULL ? path_relativo : "(null)");
                    enviar_codigo(MEM_ERROR, socket_cliente);
                }

                free(path_completo);
                list_destroy_and_destroy_elements(valores, free);
                break;
            }

            case FINISH_PROCESS: {
                atender_finish_process(socket_cliente);
                break;
            }

            case SUSPEND_PROCESS: {
                atender_suspend_process(socket_cliente);
                break;
            }

            case DESUSPEND_PROCESS: {
                atender_desuspend_process(socket_cliente);
                break;
            }

            case CREATE_SEGMENT: {
                atender_create_segment(socket_cliente);
                break;
            }

            case DELETE_SEGMENT: {
                atender_delete_segment(socket_cliente);
                break;
            }

            case MEM_WRITE: {
                atender_mem_write(socket_cliente);
                break;
            }

            case MEM_READ: {
                atender_mem_read(socket_cliente);
                break;
            }

            case REQUEST_COMPACTATION: {
                usleep(compaction_delay * 1000);
                enviar_codigo(
                    compactar_memoria() ? MEM_OK : MEM_ERROR,
                    socket_cliente
                );
                break;
            }

            case INSTRUCCION: {
                int pid = -1;
                int pc = -1;
                if (!recibir_solicitud_instruccion(socket_cliente, &pid, &pc)) {
                    log_warning(logger, "Solicitud de instruccion invalida");
                    enviar_codigo(INSTRUCCION_ERROR, socket_cliente);
                    break;
                }
                log_debug(logger, "Solicitud de instruccion - PID: %d, PC: %d", pid, pc);
                

                t_contexto* ctx = buscar_contexto(pid);
                if (ctx == NULL) {
                    log_warning(logger, "## PID: %d - Contexto no encontrado para fetch", pid);
                    enviar_codigo(INSTRUCCION_ERROR, socket_cliente);
                    break;
                }

                t_cpu_conectada* cpu = buscar_cpu_por_socket(socket_cliente);
                if (cpu != NULL)
                    cpu->pid_actual = pid;

                usleep(instruction_delay * 1000);

                log_debug(logger, "Obteniendo instruccion - path: %s", ctx ? ctx->path_instrucciones : "N/A");
                char* instruccion = obtener_instruccion(ctx->path_instrucciones, pc);
                if (instruccion == NULL) {
                    log_warning(logger,
                                "## PID: %d - No se pudo obtener instrucción para PC %d",
                                pid,
                                pc);
                    enviar_codigo(INSTRUCCION_ERROR, socket_cliente);
                    break;
                }

                log_info(logger,
                    "## PID: %d - Obtener instrucción: %d - Instrucción: %s",
                    pid, pc, instruccion
                );

                enviar_instruccion(instruccion, socket_cliente);
                free(instruccion);
                break;
            }

            case SOLICITUD_CONTEXTO: {
                int pid = recibir_pedido_contexto(socket_cliente);
                t_contexto* ctx = buscar_contexto(pid);

                if (ctx == NULL) {
                    log_warning(logger, "## PID: %d - Contexto no encontrado", pid);
                    enviar_codigo(MEM_ERROR, socket_cliente);
                    break;
                }

                t_list* tabla_cpu = list_create();
                pthread_mutex_lock(&mutex_segmentos);
                for (int i = 0; i < list_size(ctx->tabla_segmentos->segmentos); i++) {
                    t_segmento* segmento = list_get(ctx->tabla_segmentos->segmentos, i);
                    t_segmento_contexto* segmento_cpu = malloc(sizeof(t_segmento_contexto));
                    if (segmento_cpu == NULL)
                        continue;

                    segmento_cpu->id = segmento->id;
                    segmento_cpu->base = segmento->base;
                    segmento_cpu->tamanio = segmento->tamanio;
                    list_add(tabla_cpu, segmento_cpu);
                }
                pthread_mutex_unlock(&mutex_segmentos);

                enviar_contexto_ejecucion((t_registro*)&ctx->registros, socket_cliente);
                enviar_tabla_segmentos(tabla_cpu, socket_cliente);
                list_destroy_and_destroy_elements(tabla_cpu, free);
                break;
            }

            case CONTEXTO_EJEC: {
                t_cpu_conectada* cpu = buscar_cpu_por_socket(socket_cliente);
                if (cpu == NULL || cpu->pid_actual == -1) {
                    log_warning(logger, "Contexto recibido desde CPU sin PID asociado");
                    t_registro* registros_recibidos = recibir_contexto_ejecucion(socket_cliente);
                    free(registros_recibidos);
                    break;
                }

                t_registro* registros_recibidos = recibir_contexto_ejecucion(socket_cliente);
                log_debug(logger, "PID: %d - Contexto recibido desde CPU", cpu->pid_actual);
                t_contexto* ctx = buscar_contexto(cpu->pid_actual);
                if (ctx == NULL) {
                    log_warning(logger, "## PID: %d - Contexto no encontrado para actualizar", cpu->pid_actual);
                    free(registros_recibidos);
                    break;
                }

                memcpy(&ctx->registros, registros_recibidos, sizeof(t_registros));
                free(registros_recibidos);
                break;
            }

            default:
                log_warning(logger, "Operacion desconocida: %d", cod_op);
                break;
        }
    }

    liberar_conexion(socket_cliente);
    return NULL;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Uso: %s [Archivo Config]\n", argv[0]);
        return EXIT_FAILURE;
    }

    config = iniciar_config(argv[1]); //mal no se usa el argv[1] el path del archivo de config no cambia
    cargar_config(config);
    iniciar_logger();
    iniciar_estructuras();

    int server_fd = -1;
    iniciar_servidor_multiplexador(puerto_escucha, &server_fd, manejar_cliente);

    config_destroy(config);
    log_destroy(logger);
    return 0;
}
