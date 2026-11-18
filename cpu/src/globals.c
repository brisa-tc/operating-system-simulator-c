#include "globals.h"

t_log *logger;                                  // Logger para registrar eventos del módulo CPU
int socket_kernel_scheduler = -1;               // Socket de conexión con Kernel Scheduler
int socket_kernel_memory = -1;                  // Socket de conexión con Kernel Memory

int pid_actual = -1;                            // PID del proceso actualmente en ejecución
bool cpu_tiene_proceso = false;                 // Indica si la CPU tiene un proceso asignado
pthread_mutex_t mutex_estado_cpu = PTHREAD_MUTEX_INITIALIZER;

t_registro *registros;                          // Estructura con los registros de la CPU
bool fue_modificado_pc = false;                 // Indica si el PC fue modificado en la última instrucción
