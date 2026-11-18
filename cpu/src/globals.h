#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <commons/log.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/contexto_utils.h"

extern t_log *logger;                           // Logger para registrar eventos del módulo CPU
extern int socket_kernel_scheduler;             // Socket de conexión con Kernel Scheduler
extern int socket_kernel_memory;                // Socket de conexión con Kernel Memory

extern int pid_actual;                          // PID del proceso actualmente en ejecución
extern bool cpu_tiene_proceso;                  // Indica si la CPU tiene un proceso asignado
extern pthread_mutex_t mutex_estado_cpu;        // Mutex para pid_actual y cpu_tiene_proceso

extern t_registro *registros;                   // Estructura con los registros de la CPU
extern bool fue_modificado_pc;                  // Indica si el PC fue modificado en la última instrucción

extern int identificador;                       // Identificador único de la CPU

#endif
