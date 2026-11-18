#ifndef ALGORITMOS_H
#define ALGORITMOS_H

#include "types.h"
#include <stdbool.h>
#include <commons/collections/list.h>
#include <pthread.h>

// =========================
// LOG Y CONFIG (externos)
// =========================
extern t_log* logger;
extern t_config* config;

// =========================
// COLAS READY / EXEC
// =========================
extern t_list* cola_ready;
extern t_list* cola_exec;

extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_exec;

extern bool compactacion_en_progreso;
extern pthread_mutex_t mutex_compactacion;

// =========================
// CPU MANAGEMENT
// =========================
extern t_list* cpus_disponibles;
extern pthread_mutex_t mutex_cpus;
extern int proximo_cpu_id;

// =========================
// PLANIFICACIÓN GLOBAL
// =========================
extern t_algoritmo_planificacion algoritmo_largo_plazo;
extern t_algoritmo_planificacion algoritmo_corto_plazo;

// CMN (colas por prioridad)
extern int cantidad_colas_cmn;
extern t_list** colas_cmn;
extern t_algoritmo_cola* algoritmos_colas_cmn;
extern char* QUEUE_PREEMPTION;

// =========================
// PLANIFICADOR
// =========================
void inicializar_planificador_corto_plazo(void);
void* planificador_corto_plazo(void* arg);

// =========================
// READY / EXEC
// =========================
bool hay_proceso_ready(void);
t_pcb* seleccionar_siguiente_ready(t_algoritmo_cola* algoritmo_cola);
t_pcb* seleccionar_siguiente_proceso(t_algoritmo_cola* algoritmo_cola);
bool agregar_a_ready(t_pcb* pcb, t_estado estado_anterior);
bool desalojar_exec_a_ready(t_pcb* pcb);

bool agregar_a_ready_al_principio(t_pcb* pcb, t_estado estado_anterior);

// =========================
// PREEMPTION CMN
// =========================
void evaluar_desalojo_por_prioridad(t_pcb* nuevo);
t_cpu_kernel* buscar_victima_preemption_sin_lock(t_pcb* nuevo);
bool config_es_true(char* valor);

// =========================
// CPU
// =========================
t_cpu_kernel* reservar_cpu_libre(void);
void liberar_cpu(t_cpu_kernel* cpu);
void liberar_cpu_por_pid(int pid);
void liberar_cpu_por_socket(int socket_cpu);
void manejar_desconexion_cpu(int socket_cpu);
t_pcb* sacar_pcb_de_exec_por_pid(int pid);
void procesar_desalojo_cpu(int socket_cpu, int pid);
int registrar_cpu_planificador(int socket_dispatch, int socket_interrupt);
int registrar_cpu_planificador_con_id(int id_cpu, int socket_dispatch, int socket_interrupt);

// =========================
// CPU I/O (KERNEL <-> CPU)
// =========================
bool enviar_proceso_a_cpu(t_cpu_kernel* cpu, t_pcb* pcb);
bool enviar_interrupcion_a_cpu(t_cpu_kernel* cpu, t_motivo_desalojo motivo);

// =========================
// ESTADOS
// =========================
char* nombre_estado(t_estado estado);
void cambiar_estado(t_pcb* pcb, t_estado nuevo);
void loggear_cambio_estado(t_pcb* pcb, t_estado anterior, t_estado nuevo);

// =========================
// CLEANUP
// =========================
void limpiar_cpus_desconectadas(void);

#endif
