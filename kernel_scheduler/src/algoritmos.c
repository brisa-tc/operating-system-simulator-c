#include "algoritmos.h"
#include "config.h"
#include "planificadores.h"

// =========================
// DEPENDENCIAS
// =========================
#include <unistd.h>
#include <commons/log.h>
#include <utils/comunicacion.h>
#include <utils/interrupcion.h>
#include <utils/proceso.h>
#include <commons/collections/list.h>

#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

// =========================
// PROTOTIPOS INTERNOS
// =========================
static void agregar_pcb_a_ready_sin_lock(t_pcb* pcb);
static void iniciar_quantum_si_corresponde(t_pcb* pcb, t_cpu_kernel* cpu, t_algoritmo_cola algoritmo_cola);
static void* ejecutar_quantum(void* arg);
static bool lista_contiene_elemento(t_list* lista, void* elemento);
bool prioridad_valida_cmn(t_pcb* pcb);
bool config_es_true(char* valor);
bool enviar_proceso_a_cpu(t_cpu_kernel* cpu, t_pcb* pcb);

// =========================
// INICIO PLANIFICADOR
// =========================
void inicializar_planificador_corto_plazo(void) {
    pthread_t hilo;

    if (pthread_create(&hilo, NULL, planificador_corto_plazo, NULL) != 0) {
        log_error(logger, "Error creando hilo corto plazo");
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo);
}

// =========================
// READY INTERNAL
// =========================
static void agregar_pcb_a_ready_sin_lock(t_pcb* pcb) {
    if (algoritmo_corto_plazo == ALGORITMO_CMN && prioridad_valida_cmn(pcb)) {
        list_add(colas_cmn[pcb->prioridad], pcb);
    } else {
        list_add(cola_ready, pcb);
    }
}

// =========================
// ESTADOS
// =========================
char* nombre_estado(t_estado estado) {

    switch (estado) {

        case NEW:
            return "NEW";

        case READY:
            return "READY";

        case EXEC:
            return "EXEC";

        case BLOCK:
            return "BLOCK";

        case SUSP_BLOCK:
            return "SUSP. BLOCK";

        case SUSP_READY:
            return "SUSP. READY";

        case EXIT:
            return "EXIT";

        default:
            return "DESCONOCIDO";
    }
}

// =========================
// CAMBIO DE ESTADO
// =========================
void loggear_cambio_estado(t_pcb* pcb, t_estado ant, t_estado nuevo) {

    log_info(logger,
        "## (%d) Pasa del estado %s al estado %s",
        pcb->pid,
        nombre_estado(ant),
        nombre_estado(nuevo)
    );
}

void cambiar_estado(t_pcb* pcb, t_estado nuevo) {

    t_estado anterior = pcb->estado;

    pcb->estado = nuevo;

    loggear_cambio_estado(pcb, anterior, nuevo);
}



// =========================
// UTIL
// =========================
bool config_es_true(char* valor) {
    return valor != NULL && strcasecmp(valor, "TRUE") == 0;
}

static bool lista_contiene_elemento(t_list* lista, void* elemento) {
    for (int i = 0; i < list_size(lista); i++) {
        if (list_get(lista, i) == elemento) {
            return true;
        }
    }

    return false;
}

// =========================
// READY
// =========================
bool prioridad_valida_cmn(t_pcb* pcb) {
    return pcb->prioridad >= 0 && pcb->prioridad < cantidad_colas_cmn;
}

bool hay_proceso_ready() {
    pthread_mutex_lock(&mutex_ready);

    bool hay = false;

    if (algoritmo_corto_plazo == ALGORITMO_CMN) {
        for (int i = 0; i < cantidad_colas_cmn; i++) {
            if (!list_is_empty(colas_cmn[i])) {
                hay = true;
                break;
            }
        }
    } else {
        hay = !list_is_empty(cola_ready);
    }

    pthread_mutex_unlock(&mutex_ready);
    return hay;
}

t_pcb* seleccionar_siguiente_ready(t_algoritmo_cola* algoritmo_cola) {
    pthread_mutex_lock(&mutex_ready);

    if (algoritmo_corto_plazo == ALGORITMO_CMN) {
        for (int i = 0; i < cantidad_colas_cmn; i++) {
            if (!list_is_empty(colas_cmn[i])) {
                t_pcb* pcb = list_remove(colas_cmn[i], 0);
                *algoritmo_cola = algoritmos_colas_cmn[i];
                pthread_mutex_unlock(&mutex_ready);
                return pcb;
            }
        }

        pthread_mutex_unlock(&mutex_ready);
        return NULL;
    }

    if (list_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_ready);
        return NULL;
    }

    t_pcb* pcb = list_remove(cola_ready, 0);
    *algoritmo_cola = (algoritmo_corto_plazo == ALGORITMO_RR) ? COLA_RR : COLA_FIFO;

    pthread_mutex_unlock(&mutex_ready);
    return pcb;
}

t_pcb* seleccionar_siguiente_proceso(t_algoritmo_cola* algoritmo_cola) {
    return seleccionar_siguiente_ready(algoritmo_cola);
}

bool agregar_a_ready(t_pcb* pcb, t_estado estado_anterior) {
    if (pcb == NULL) return false;

    pcb->estado = READY;

    pthread_mutex_lock(&mutex_ready);

    if (algoritmo_corto_plazo == ALGORITMO_CMN && !prioridad_valida_cmn(pcb)) {
        list_add(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_ready);

        loggear_cambio_estado(pcb, estado_anterior, READY);
        log_warning(logger, "## (%d) Prioridad inválida", pcb->pid);
        return false;
    }

    agregar_pcb_a_ready_sin_lock(pcb);

    pthread_mutex_unlock(&mutex_ready);

    loggear_cambio_estado(pcb, estado_anterior, READY);

    evaluar_desalojo_por_prioridad(pcb);

    return true;
}

bool agregar_a_ready_al_principio(t_pcb* pcb, t_estado estado_anterior) {
    if (pcb == NULL) return false;

    pcb->estado = READY;

    pthread_mutex_lock(&mutex_ready);

    if (algoritmo_corto_plazo == ALGORITMO_CMN && prioridad_valida_cmn(pcb)) {
        list_add_in_index(colas_cmn[pcb->prioridad], 0, pcb);
    } else {
        list_add_in_index(cola_ready, 0, pcb);
    }

    pthread_mutex_unlock(&mutex_ready);

    loggear_cambio_estado(pcb, estado_anterior, READY);
    return true;
}

// =========================
// CPU
// =========================
t_cpu_kernel* reservar_cpu_libre() {
    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->libre && cpu->socket_dispatch != -1) {
            cpu->libre = false;
            pthread_mutex_unlock(&mutex_cpus);
            return cpu;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
    return NULL;
}

void liberar_cpu(t_cpu_kernel* cpu) {
    pthread_mutex_lock(&mutex_cpus);
    cpu->libre = cpu->socket_dispatch != -1;
    cpu->pcb = NULL;
    pthread_mutex_unlock(&mutex_cpus);
}

void liberar_cpu_por_pid(int pid) {
    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->pcb && cpu->pcb->pid == pid) {
            cpu->pcb = NULL;
            cpu->libre = cpu->socket_dispatch != -1;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
}

void liberar_cpu_por_socket(int socket_cpu) {
    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->socket_dispatch == socket_cpu || cpu->socket_interrupt == socket_cpu) {
            cpu->pcb = NULL;
            cpu->libre = cpu->socket_dispatch != -1;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
}

void manejar_desconexion_cpu(int socket_cpu) {
    t_pcb* pcb_asignado = NULL;
    int id_cpu = -1;

    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->socket_dispatch == socket_cpu || cpu->socket_interrupt == socket_cpu) {
            id_cpu = cpu->id;
            pcb_asignado = cpu->pcb;
            cpu->socket_dispatch = -1;
            cpu->socket_interrupt = -1;
            cpu->libre = false;
            cpu->pcb = NULL;
            cpu->tiene_interrupcion_pendiente = false;
            cpu->desconectado_en = time(NULL);
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);

    if (id_cpu == -1)
        return;

    log_warning(logger, "## CPU %d Desconectada", id_cpu);

    if (pcb_asignado == NULL)
        return;

    t_pcb* pcb = sacar_pcb_de_exec_por_pid(pcb_asignado->pid);
    if (pcb != NULL)
        agregar_a_ready(pcb, EXEC);
}

t_pcb* sacar_pcb_de_exec_por_pid(int pid) {
    pthread_mutex_lock(&mutex_exec);

    for (int i = 0; i < list_size(cola_exec); i++) {
        t_pcb* pcb = list_get(cola_exec, i);

        if (pcb->pid == pid) {
            list_remove(cola_exec, i);
            pthread_mutex_unlock(&mutex_exec);
            return pcb;
        }
    }

    pthread_mutex_unlock(&mutex_exec);
    return NULL;
}

void procesar_desalojo_cpu(int socket_cpu, int pid) {
    t_motivo_desalojo motivo = MOTIVO_FIN_QUANTUM;

    pthread_mutex_lock(&mutex_cpus);

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->socket_dispatch == socket_cpu || cpu->socket_interrupt == socket_cpu) {
            motivo = cpu->motivo_interrupcion;
            cpu->tiene_interrupcion_pendiente = false;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cpus);

    t_pcb* pcb = sacar_pcb_de_exec_por_pid(pid);

    if (pcb == NULL) {
        log_warning(logger, "CPU devolvió PID %d, pero no estaba en EXEC", pid);
        liberar_cpu_por_socket(socket_cpu);
        return;
    }

    liberar_cpu_por_socket(socket_cpu);

    switch (motivo) {
        case MOTIVO_FIN_QUANTUM:
            log_info(logger, "## (%d) - Desalojado por fin de quantum", pid);
            agregar_a_ready(pcb, EXEC);
            break;

        case MOTIVO_PRIORIDAD:
            agregar_a_ready(pcb, EXEC);
            break;

        case MOTIVO_COMPACTACION:
            agregar_a_ready_al_principio(pcb, EXEC);
            break;

        case MOTIVO_SEG_FAULT:
            pasar_a_exit_con_motivo(pcb, "SEG_FAULT");
            break;

        default:
            log_warning(logger, "Motivo de desalojo desconocido para PID %d", pid);
            agregar_a_ready(pcb, EXEC);
            break;
    }
}


int registrar_cpu_planificador(int socket_dispatch, int socket_interrupt) {
    return registrar_cpu_planificador_con_id(proximo_cpu_id++, socket_dispatch, socket_interrupt);
}

int registrar_cpu_planificador_con_id(int id_cpu, int socket_dispatch, int socket_interrupt) {
    pthread_mutex_lock(&mutex_cpus);

    t_cpu_kernel* cpu = malloc(sizeof(t_cpu_kernel));

    if (cpu == NULL) {
        pthread_mutex_unlock(&mutex_cpus);
        log_error(logger, "No se pudo reservar memoria para CPU");
        return -1;
    }

    cpu->id = id_cpu;
    cpu->socket_dispatch = socket_dispatch;
    cpu->socket_interrupt = socket_interrupt;
    cpu->libre = true;
    cpu->pcb = NULL;
    cpu->tiene_interrupcion_pendiente = false;
    cpu->motivo_interrupcion = MOTIVO_FIN_QUANTUM;
    cpu->generacion_despacho = 0;

    list_add(cpus_disponibles, cpu);

    pthread_mutex_unlock(&mutex_cpus);

    log_info(logger, "## CPU %d Conectada", id_cpu);

    return id_cpu;
}

// =========================
// INTERRUPCIÓN
// =========================
bool enviar_interrupcion_a_cpu(t_cpu_kernel* cpu, t_motivo_desalojo motivo) {
    if (cpu == NULL || cpu->socket_interrupt == -1) {
        log_error(logger, "No se pudo enviar interrupción: CPU inválida");
        return false;
    }

    int pid;
    pthread_mutex_lock(&mutex_cpus);
    cpu->tiene_interrupcion_pendiente = true;
    cpu->motivo_interrupcion = motivo;
    pid = cpu->pcb != NULL ? cpu->pcb->pid : -1;
    pthread_mutex_unlock(&mutex_cpus);

    enviar_interrupcion(pid, true, cpu->socket_interrupt);

    return true;
}

// =========================
// ENVÍO CPU
// =========================
bool enviar_proceso_a_cpu(t_cpu_kernel* cpu, t_pcb* pcb) {
    if (cpu == NULL || pcb == NULL || cpu->socket_dispatch == -1) {
        log_error(logger, "No se pudo enviar proceso a CPU real");
        return false;
    }

    t_paquete* paquete = crear_paquete();

    paquete->codigo_operacion = EJECUTAR_PROCESO;

    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));

    enviar_paquete(paquete, cpu->socket_dispatch);

    eliminar_paquete(paquete);

    return true;
}


static void iniciar_quantum_si_corresponde(t_pcb* pcb, t_cpu_kernel* cpu, t_algoritmo_cola algoritmo_cola) {
    if (algoritmo_cola != COLA_RR) {
        return;
    }

    t_quantum_args* args = malloc(sizeof(t_quantum_args));
    if (args == NULL) {
        log_error(logger, "No se pudo reservar memoria para quantum");
        return;
    }

    args->pcb = pcb;
    args->cpu = cpu;

    pthread_mutex_lock(&mutex_cpus);
    args->generacion_despacho = cpu->generacion_despacho;
    pthread_mutex_unlock(&mutex_cpus);

    pthread_t hilo_quantum;
    if (pthread_create(&hilo_quantum, NULL, ejecutar_quantum, args) != 0) {
        log_error(logger, "Error creando hilo de quantum");
        free(args);
        return;
    }

    pthread_detach(hilo_quantum);
}

static void* ejecutar_quantum(void* arg) {
    t_quantum_args* args = (t_quantum_args*) arg;
    t_pcb* pcb = args->pcb;
    t_cpu_kernel* cpu = args->cpu;

    usleep(RR_QUANTUM * 1000);

    pthread_mutex_lock(&mutex_cpus);
    bool despacho_vigente = !cpu->libre &&
        cpu->socket_dispatch != -1 &&
        cpu->pcb == pcb &&
        cpu->generacion_despacho == args->generacion_despacho;
    pthread_mutex_unlock(&mutex_cpus);

    if (!despacho_vigente) {
        free(args);
        return NULL;
    }

    pthread_mutex_lock(&mutex_exec);
    bool sigue_en_exec = lista_contiene_elemento(cola_exec, pcb) && pcb->estado == EXEC;
    pthread_mutex_unlock(&mutex_exec);

    if (sigue_en_exec) {
        enviar_interrupcion_a_cpu(cpu, MOTIVO_FIN_QUANTUM);
    }

    free(args);
    return NULL;
}

t_cpu_kernel* buscar_victima_preemption_sin_lock(t_pcb* nuevo) {
    t_cpu_kernel* victima = NULL;

    for (int i = 0; i < list_size(cpus_disponibles); i++) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->libre || cpu->pcb == NULL)
            continue;

        if (cpu->pcb->prioridad > nuevo->prioridad) {
            if (victima == NULL || cpu->pcb->prioridad > victima->pcb->prioridad) {
                victima = cpu;
            }
        }
    }

    return victima;
}

// =========================
// PREEMPTION
// =========================
void evaluar_desalojo_por_prioridad(t_pcb* nuevo) {
    if (algoritmo_corto_plazo != ALGORITMO_CMN || !config_es_true(QUEUE_PREEMPTION))
        return;

    if (!prioridad_valida_cmn(nuevo))
        return;

    pthread_mutex_lock(&mutex_cpus);

    t_cpu_kernel* victima = buscar_victima_preemption_sin_lock(nuevo);

    if (victima == NULL) {
        pthread_mutex_unlock(&mutex_cpus);
        return;
    }

    t_pcb* pcb_victima = victima->pcb;

    pthread_mutex_unlock(&mutex_cpus);

    enviar_interrupcion_a_cpu(victima, MOTIVO_PRIORIDAD);

    log_info(logger,
        "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
        pcb_victima->pid,
        pcb_victima->prioridad,
        nuevo->pid,
        nuevo->prioridad
    );
}


// =========================
// EXEC -> READY
// =========================
bool desalojar_exec_a_ready(t_pcb* pcb) {
    if (pcb == NULL) return false;

    pthread_mutex_lock(&mutex_exec);
    bool ok = list_remove_element(cola_exec, pcb);
    pthread_mutex_unlock(&mutex_exec);

    if (!ok || pcb->estado != EXEC)
        return false;

    liberar_cpu_por_pid(pcb->pid);

    return agregar_a_ready(pcb, EXEC);
}

// =========================
// CLEANUP CPUs DESCONECTADAS
// =========================
void limpiar_cpus_desconectadas(void) {
    time_t ahora = time(NULL);
    int grace_period = (RR_QUANTUM / 1000) + 2;

    pthread_mutex_lock(&mutex_cpus);

    for (int i = list_size(cpus_disponibles) - 1; i >= 0; i--) {
        t_cpu_kernel* cpu = list_get(cpus_disponibles, i);

        if (cpu->socket_dispatch == -1 && cpu->socket_interrupt == -1 && cpu->pcb == NULL) {
            if (difftime(ahora, cpu->desconectado_en) >= grace_period) {
                list_remove(cpus_disponibles, i);
                free(cpu);
            }
        }
    }

    pthread_mutex_unlock(&mutex_cpus);
}

// =========================
// PLANIFICADOR
// =========================
void* planificador_corto_plazo(void* arg) {

    (void)arg;

    while (1) {

        pthread_mutex_lock(&mutex_compactacion);
        bool compactando = compactacion_en_progreso;
        pthread_mutex_unlock(&mutex_compactacion);

        if (compactando) {
            usleep(100000);
            continue;
        }

        limpiar_cpus_desconectadas();
        
        if (!hay_proceso_ready()) {
            usleep(100000);
            continue;
        }

        t_cpu_kernel* cpu = reservar_cpu_libre();

        if (cpu == NULL) {
            usleep(100000);
            continue;
        }

        t_algoritmo_cola alg = COLA_FIFO;
        t_pcb* pcb = seleccionar_siguiente_ready(&alg);

        if (pcb == NULL) {
            liberar_cpu(cpu);
            continue;
        }

        if (!enviar_proceso_a_cpu(cpu, pcb)) {
            agregar_a_ready(pcb, EXEC);
            liberar_cpu(cpu);
            continue;
        }

        cambiar_estado(pcb, EXEC);

        pthread_mutex_lock(&mutex_exec);
        list_add(cola_exec, pcb);
        pthread_mutex_unlock(&mutex_exec);

        pthread_mutex_lock(&mutex_cpus);
        bool cpu_desconectada = cpu->socket_dispatch == -1;
        if (!cpu_desconectada) {
            cpu->pcb = pcb;
            cpu->generacion_despacho++;
        }
        pthread_mutex_unlock(&mutex_cpus);

        if (cpu_desconectada) {
            pthread_mutex_lock(&mutex_exec);
            list_remove_element(cola_exec, pcb);
            pthread_mutex_unlock(&mutex_exec);
            agregar_a_ready(pcb, EXEC);
            continue;
        }

        iniciar_quantum_si_corresponde(pcb, cpu, alg);
    }

    return NULL;
}
