#ifndef INIT_H
#define INIT_H
#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdbool.h>
#include "types.h"

// LOG Y CONFIG
extern t_log* logger;
extern t_config* config;

// PLANIFICACION
extern t_algoritmo_planificacion algoritmo_largo_plazo;
extern t_algoritmo_planificacion algoritmo_corto_plazo;

extern int cantidad_colas_cmn;
extern t_list** colas_cmn;
extern t_algoritmo_cola* algoritmos_colas_cmn;

// CPUs
extern t_list* cpus_disponibles;
extern pthread_mutex_t mutex_cpus;
extern int proximo_cpu_id;

// IO
extern t_list* ios_conectadas;
extern pthread_mutex_t mutex_ios;

// COLAS DE PLANIFICACION
extern t_list* cola_new;
extern t_list* cola_ready;
extern t_list* cola_exec;
extern t_list* cola_block;
extern t_list* cola_exit;

extern t_list* cola_susp_block;
extern t_list* cola_susp_ready;

extern t_list* lista_block_timers;

// MUTEX
extern pthread_mutex_t mutex_new;
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_exec;
extern pthread_mutex_t mutex_block;

extern pthread_mutex_t mutex_block_timers;
extern pthread_mutex_t mutex_susp_block;
extern pthread_mutex_t mutex_susp_ready;

extern pthread_mutex_t mutex_exit;

void iniciar_modulo(char* path_config, char* path_proceso_inicial);

#endif
