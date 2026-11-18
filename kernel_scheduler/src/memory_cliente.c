#include "memory_cliente.h"
#include <utils/comunicacion.h>
#include <utils/proceso.h>
#include <utils/identificadores.h>
#include "config.h"
#include "planificadores.h"
#include "algoritmos.h"
#include "init.h"
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

extern t_log* logger;

int conexion_memoria;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

bool memoria_ampliada_pendiente = false;

static int recibir_respuesta_memoria_local(int socket) {
    while (1) {
        int op = recibir_operacion(socket);
        if (op == MEM_AMPLIADA) {
            memoria_ampliada_pendiente = true;
            continue;
        }
        return op;
    }
}

static void atender_bsod_memoria(void) {
    log_error(logger, "## Se detecto corrupcion de memoria - BSOD");
    finalizar_todos_los_procesos();
    log_error(logger, "## Kernel Scheduler finalizado por BSOD");
    sleep(1);
    exit(EXIT_FAILURE);
}

static void* escuchar_bsod_memoria(void* arg) {
    int socket_memoria = *(int*) arg;

    while (1) {
        int op;
        int bytes = recv(socket_memoria, &op, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

        if (bytes == 0) {
            return NULL;
        }

        if (bytes < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return NULL;
            }

            usleep(100000);
            continue;
        }

        if (bytes == sizeof(int) && op == BSOD) {
            recibir_operacion(socket_memoria);
            atender_bsod_memoria();
            return NULL;
        }

        usleep(100000);
    }
}

void conectar_con_memoria() {

    conexion_memoria = crear_conexion(
        IP_KERNEL_MEMORY,
        PUERTO_KERNEL_MEMORY
    );

    if (conexion_memoria == -1) {
        log_error(logger,
            "No se pudo conectar a Kernel Memory");
        exit(EXIT_FAILURE);
    }

    enviar_identificador(
        MENSAJE_SCHEDULER,
        conexion_memoria
    );

    log_info(logger,
        "## Conectado a Kernel Memory");

    pthread_t hilo_bsod_memoria;
    if (pthread_create(&hilo_bsod_memoria, NULL, escuchar_bsod_memoria, &conexion_memoria) != 0) {
        log_error(logger, "Error creando hilo de escucha BSOD de Kernel Memory");
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_bsod_memoria);
}

bool memoria_solicitar_compactacion(void) {
    pthread_mutex_lock(&mutex_memoria);
    enviar_codigo(REQUEST_COMPACTATION, conexion_memoria);

    int respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    return respuesta == MEM_OK;
}

bool memoria_crear_proceso(int pid, char* path) {
    pthread_mutex_lock(&mutex_memoria);
    enviar_crear_proceso(conexion_memoria, pid, path);

    int respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    if (respuesta == MEM_OK) {
        return true;
    }

    log_error(logger, "Error creando proceso PID %d en Kernel Memory", pid);
    return false;
}

bool memoria_finalizar_proceso(int pid) {
    pthread_mutex_lock(&mutex_memoria);
    enviar_finalizar_proceso(conexion_memoria, pid);

    int respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    if (respuesta == MEM_OK) {
        return true;
    }

    log_error(logger, "Error finalizando proceso PID %d en Kernel Memory", pid);
    return false;
}

bool memoria_desuspender_proceso(int pid) {
    pthread_mutex_lock(&mutex_memoria);

    enviar_desuspender_proceso(
        conexion_memoria,
        pid
    );

    int respuesta =
        recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    if (respuesta == MEM_OK) {
        log_debug(logger, "## (%d) Memory OK", pid);
        return true;
    }

    log_debug(logger, "## (%d) Memory FULL", pid);
    return false;
}

bool memoria_suspender_proceso(int pid) {
    pthread_mutex_lock(&mutex_memoria);
    enviar_suspender_proceso(
        conexion_memoria,
        pid
    );

    int respuesta =
        recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    return respuesta == MEM_OK;
}

bool memoria_eliminar_segmento(int pid, int id_segmento) {
    pthread_mutex_lock(&mutex_memoria);
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = DELETE_SEGMENT;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &id_segmento, sizeof(int));

    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    if (respuesta == MEM_OK) {
        return true;
    }

    log_error(logger,
        "Error eliminando segmento PID %d SEG %d",
        pid,
        id_segmento
    );

    return false;
}

op_code memoria_crear_segmento(int pid, int id_segmento, int tamanio) {
    pthread_mutex_lock(&mutex_memoria);
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CREATE_SEGMENT;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &id_segmento, sizeof(int));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));

    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    op_code respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    return respuesta;
}

bool memoria_escribir(int pid, uint32_t direccion_logica, uint32_t tamanio, char* contenido) {
    pthread_mutex_lock(&mutex_memoria);
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = MEM_WRITE;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(paquete, contenido, strlen(contenido) + 1);

    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_respuesta_memoria_local(conexion_memoria);
    pthread_mutex_unlock(&mutex_memoria);

    return respuesta == MEM_OK;
}

char* memoria_leer(int pid, uint32_t direccion_logica, uint32_t tamanio) {
    pthread_mutex_lock(&mutex_memoria);
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = MEM_READ;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(conexion_memoria);
    if (respuesta != MEM_READ) {
        pthread_mutex_unlock(&mutex_memoria);
        log_warning(logger, "Kernel Memory no pudo leer PID %d - respuesta %d", pid, respuesta);
        return NULL;
    }

    t_list* valores = recibir_paquete(conexion_memoria);
    char* contenido = strdup((char*)list_get(valores, 0));

    list_destroy_and_destroy_elements(valores, free);
    pthread_mutex_unlock(&mutex_memoria);
    return contenido;
}

void manejar_ampliacion_memoria(void) {
    t_pcb* seleccionado = NULL;

    pthread_mutex_lock(&mutex_susp_ready);
    for (int i = 0; i < list_size(cola_susp_ready); i++) {
        t_pcb* pcb = list_get(cola_susp_ready, i);
        if (seleccionado == NULL || comparar_prioridad_y_tiempo(pcb, seleccionado))
            seleccionado = pcb;
    }

    if (seleccionado != NULL)
        list_remove_element(cola_susp_ready, seleccionado);
    pthread_mutex_unlock(&mutex_susp_ready);

    if (seleccionado == NULL) {
        log_debug(logger, "## No hay procesos en SUSP_READY para desuspender por ampliación");
        return;
    }

    log_info(logger, "## PID: %d - Desuspendiendo por ampliación de memoria", seleccionado->pid);

    if (memoria_desuspender_proceso(seleccionado->pid)) {
        seleccionado->intentos_memoria = 0;
        agregar_a_ready(seleccionado, SUSP_READY);
        log_info(logger, "## PID: %d - Desuspendido exitosamente por ampliación de memoria",
                 seleccionado->pid);
    } else {
        log_warning(logger, "## PID: %d - No se pudo desuspender por ampliación, re-agregando a SUSP_READY",
                    seleccionado->pid);
        pthread_mutex_lock(&mutex_susp_ready);
        list_add(cola_susp_ready, seleccionado);
        pthread_mutex_unlock(&mutex_susp_ready);
    }
}
