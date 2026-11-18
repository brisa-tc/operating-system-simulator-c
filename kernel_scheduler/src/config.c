#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include "config.h"


// =========================
// VARIABLES GLOBALES

extern t_log* logger;
t_config* config;
char* PUERTO_ESCUCHA;

char* PLANIFICATION_ALGORITHM;
char** QUEUES_ALGORITHMS;
int RR_QUANTUM;
char* QUEUE_PREEMPTION;
int SUSPENSION_TIMEOUT;
t_log_level LOG_LEVEL;

char* IP_KERNEL_MEMORY;
char* PUERTO_KERNEL_MEMORY;

// =========================
// INICIAR CONFIG


t_config* iniciar_config(char* path_config) {

    config = config_create(path_config);

    if (config == NULL) {
        printf("No se pudo cargar el config: %s\n", path_config);
        exit(EXIT_FAILURE);
    }

    return config;
}


// =========================
// CARGAR VARIABLES GLOBALES

void cargar_configuraciones(t_config* config) {
    
    PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");

    PLANIFICATION_ALGORITHM = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    QUEUES_ALGORITHMS = config_has_property(config, "QUEUES_ALGORITHMS")
        ? config_get_array_value(config, "QUEUES_ALGORITHMS")
        : NULL;
    RR_QUANTUM = config_get_int_value(config, "RR_QUANTUM");
    QUEUE_PREEMPTION = config_get_string_value(config, "QUEUE_PREEMPTION");
    SUSPENSION_TIMEOUT = config_get_int_value(config, "SUSPENSION_TIMEOUT");

    LOG_LEVEL = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));

    IP_KERNEL_MEMORY = config_get_string_value(config, "IP_KERNEL_MEMORY");
    PUERTO_KERNEL_MEMORY = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
}



// =========================
// INICIAR LOGGER


void iniciar_logger() {
    //aca abajo antes estaba asi: logger = log_create("kernel_scheduler.log", "KS", true, log_level_from_string(config_get_string_value(config, "LOG_LEVEL")));. pero ahora lo cambie por lo que esta abajo
    logger = log_create("kernel_scheduler.log", "KS", true, LOG_LEVEL);

    if (logger == NULL) {
        printf("Error al crear logger\n");
        exit(EXIT_FAILURE);
    }
}


// =========================
// LOGGEAR CONFIG

void loggear_config() {

    log_debug(logger, "Algoritmo de planificacion: %s", PLANIFICATION_ALGORITHM);
    if (QUEUES_ALGORITHMS != NULL) {
        for (int i = 0; QUEUES_ALGORITHMS[i] != NULL; i++) {
            log_debug(logger, "Cola CMN %d: %s", i, QUEUES_ALGORITHMS[i]);
        }
    }
    log_debug(logger, "Quantum: %d", RR_QUANTUM);
    log_debug(logger, "Preemption: %s", QUEUE_PREEMPTION);
    log_debug(logger, "Suspension timeout: %d", SUSPENSION_TIMEOUT);
}
