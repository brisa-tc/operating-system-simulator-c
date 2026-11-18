#include <unistd.h>
#include "globals.h"
#include "conexiones.h"

// Variables globales para conexiones y memoria
t_list* lista_memory_sticks = NULL;
pthread_mutex_t mutex_memory_sticks = PTHREAD_MUTEX_INITIALIZER;

t_list* tabla_segmentos_actual = NULL;
pthread_mutex_t mutex_tabla_segmentos = PTHREAD_MUTEX_INITIALIZER;

int tam_max_segmento_cpu = 0;

// Variables para manejo de contexto y ciclo de instrucción
static pthread_mutex_t mutex_contexto = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_contexto = PTHREAD_COND_INITIALIZER;
static bool registros_contexto_recibidos = false;
static bool tabla_contexto_recibida = false;
static bool contexto_error = false;

static bool solicitar_y_esperar_contexto(int pid) {
    pthread_mutex_lock(&mutex_contexto);
    registros_contexto_recibidos = false;
    tabla_contexto_recibida = false;
    contexto_error = false;
    pthread_mutex_unlock(&mutex_contexto);

    enviar_pedido_contexto(pid, socket_kernel_memory);

    pthread_mutex_lock(&mutex_contexto);
    while ((!registros_contexto_recibidos || !tabla_contexto_recibida) && !contexto_error) {
        pthread_cond_wait(&cond_contexto, &mutex_contexto);
    }
    bool contexto_completo = !contexto_error;
    pthread_mutex_unlock(&mutex_contexto);

    return contexto_completo;
}

void* atender_servidor(void* arg) {
    int socket_servidor = *(int*)arg;
    while (1) {
        int op = recibir_operacion(socket_servidor);

        if (op == -1) {
            log_info(logger, "servidor desconectado");
            return NULL;
        }

        switch (op) {
            //casos para kernel scheduler
            case EJECUTAR_PROCESO: 
                t_list* valores = recibir_paquete(socket_servidor);
                int pid = *(int*) list_get(valores, 0);
                list_destroy_and_destroy_elements(valores, free);

                if (!solicitar_y_esperar_contexto(pid)) {
                    log_error(logger,
                              "## PID: %d - Kernel Memory no pudo entregar el contexto",
                              pid);
                    enviar_error_cpu(pid, socket_kernel_scheduler);
                    break;
                }

                pthread_mutex_lock(&mutex_estado_cpu);
                pid_actual = pid;
                cpu_tiene_proceso = true;
                pthread_mutex_unlock(&mutex_estado_cpu);
                log_info(logger, "CPU recibió PID %d para ejecutar", pid);

                pthread_mutex_lock(&mutex_ciclo);
                ciclo_activo = true;
                pthread_cond_signal(&cond_ciclo);
                pthread_mutex_unlock(&mutex_ciclo);

                break;
            case INTERRUPCION_CPU:
                log_info(logger, "## Interrupción recibida");
                int nuevo_pid;
                bool esDesalojo;
                recibir_interrupcion(socket_servidor, &nuevo_pid, &esDesalojo);

                pthread_mutex_lock(&mutex_estado_cpu);
                bool interrupcion_valida = cpu_tiene_proceso && nuevo_pid == pid_actual;
                pthread_mutex_unlock(&mutex_estado_cpu);

                if (!interrupcion_valida) {
                    log_debug(logger, "Interrupción descartada para PID %d", nuevo_pid);
                    break;
                }

                pthread_mutex_lock(&mutex_lista_interrupciones);
                if (lista_interrupciones == NULL) {
                    lista_interrupciones = list_create();
                }
                t_registro_interrupcion* reg = malloc(sizeof(t_registro_interrupcion));
                reg->pid = nuevo_pid;
                reg->es_desalojo = esDesalojo;
                list_add(lista_interrupciones, reg);
                pthread_mutex_unlock(&mutex_lista_interrupciones);
                break;
            case INSTRUCCION:
                {
                    char* inst = recibir_instruccion(socket_servidor);
                    pthread_mutex_lock(&mutex_instruccion);
                    if (buffer_instruccion != NULL) free(buffer_instruccion);
                    buffer_instruccion = inst;
                    op_code_instruccion = INSTRUCCION;
                    respuesta_instruccion_lista = true;
                    pthread_cond_signal(&cond_instruccion);
                    pthread_mutex_unlock(&mutex_instruccion);
                }
                break;
            case INSTRUCCION_ERROR:
                pthread_mutex_lock(&mutex_instruccion);
                if (buffer_instruccion != NULL) {
                    free(buffer_instruccion);
                    buffer_instruccion = NULL;
                }
                op_code_instruccion = INSTRUCCION_ERROR;
                respuesta_instruccion_lista = true;
                pthread_cond_signal(&cond_instruccion);
                pthread_mutex_unlock(&mutex_instruccion);
                break;
            case CONTEXTO_EJEC: {
                    t_registro* nuevos_registros = recibir_contexto_ejecucion(socket_servidor);

                    pthread_mutex_lock(&mutex_estado_cpu);
                    if (registros != NULL) free(registros);
                    registros = nuevos_registros;
                    pthread_mutex_unlock(&mutex_estado_cpu);

                    log_debug(logger, "CPU recibió contexto de ejecución");

                    pthread_mutex_lock(&mutex_contexto);
                    registros_contexto_recibidos = true;
                    if (tabla_contexto_recibida)
                        pthread_cond_signal(&cond_contexto);
                    pthread_mutex_unlock(&mutex_contexto);
                }
                break;
            case SEGMENT_MAX_SIZE: {
                    tam_max_segmento_cpu = recibir_segment_max_size(socket_servidor);
                    log_debug(logger, "CPU recibió segment_max_size: %d", tam_max_segmento_cpu);
                }
                break;
            case TABLA_SEGMENTOS: {
                    pthread_mutex_lock(&mutex_tabla_segmentos);
                    if (tabla_segmentos_actual != NULL)
                        list_destroy_and_destroy_elements(tabla_segmentos_actual, free);
                    tabla_segmentos_actual = recibir_tabla_segmentos(socket_servidor);
                    pthread_mutex_unlock(&mutex_tabla_segmentos);

                    log_debug(logger, "CPU recibió tabla de segmentos (%d elementos)",
                              tabla_segmentos_actual ? list_size(tabla_segmentos_actual) : 0);

                    pthread_mutex_lock(&mutex_contexto);
                    tabla_contexto_recibida = true;
                    if (registros_contexto_recibidos)
                        pthread_cond_signal(&cond_contexto);
                    pthread_mutex_unlock(&mutex_contexto);
                }
                break;
            case MEM_ERROR:
                pthread_mutex_lock(&mutex_contexto);
                contexto_error = true;
                pthread_cond_signal(&cond_contexto);
                pthread_mutex_unlock(&mutex_contexto);
                break;
            case SYSCALL:
                break;
            case LIST_MEMORY_STICKS:
                {
                    pthread_mutex_lock(&mutex_memory_sticks);
                    if (lista_memory_sticks != NULL) {
                        for (int i = 0; i < list_size(lista_memory_sticks); i++) {
                            t_memoryStick_info* info = list_get(lista_memory_sticks, i);
                            if (info->socket > 0) {
                                close(info->socket);
                            }
                        }
                        eliminar_lista_memory_sticks(lista_memory_sticks);
                        lista_memory_sticks = NULL;
                    }
                    pthread_mutex_unlock(&mutex_memory_sticks);

                    t_list* nueva_lista = recibir_lista_memory_sticks(socket_servidor);

                    uint32_t base_acumulada = 0;
                    for (int i = 0; i < list_size(nueva_lista); i++) {
                        t_memoryStick_info* info = list_get(nueva_lista, i);
                        info->socket = crear_conexion(info->ip, info->puerto);
                        enviar_identificador(MENSAJE_CPU, info->socket);
                        enviar_identificador(identificador, info->socket);
                        info->base = base_acumulada;
                        base_acumulada += info->tamanio;
                    }

                    pthread_mutex_lock(&mutex_memory_sticks);
                    lista_memory_sticks = nueva_lista;
                    pthread_mutex_unlock(&mutex_memory_sticks);

                    log_debug(logger, "CPU conectado a %d memory sticks", list_size(nueva_lista));
                }
                break;
            case MENSAJE:
                recibir_mensaje(socket_servidor);
                break;
            default:
                log_warning(logger, "Operacion desconocida: %d", op);
                if (socket_servidor == socket_kernel_memory) {
                    pthread_mutex_lock(&mutex_instruccion);
                    if (buffer_instruccion != NULL) free(buffer_instruccion);
                    op_code_instruccion = op;
                    buffer_instruccion = NULL;
                    respuesta_instruccion_lista = true;
                    pthread_cond_signal(&cond_instruccion);
                    pthread_mutex_unlock(&mutex_instruccion);
                }
                break;
            }
    }
    return NULL;
}

void* esperar_respuesta(int socket_servidor) {
    int op = recibir_operacion(socket_servidor);

    if (op == -1) {
        log_info(logger, "servidor desconectado");
        return NULL;
    }

    switch (op) {
        case LECTURA:
            return (void*) recibir_respuesta_lectura(socket_servidor);
        case ESCRITURA:
            return (void*) recibir_respuesta_escritura(socket_servidor);
        default:
            log_warning(logger, "Operacion desconocida: %d", op);
            return NULL;
    }
}
