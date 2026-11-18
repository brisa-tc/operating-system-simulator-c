#ifndef CONFIG_H
#define CONFIG_H

#include <commons/log.h>
#include <commons/config.h>

// VARIABLES GLOBALES 
//-------------------------------
  //Config y Logger
extern t_config* config;
extern t_log* logger;

  //Configuracion del sistema 
extern char* PLANIFICATION_ALGORITHM;
extern char** QUEUES_ALGORITHMS;
extern int RR_QUANTUM;
extern char* QUEUE_PREEMPTION;
extern int SUSPENSION_TIMEOUT;
extern t_log_level LOG_LEVEL;

  //Conexiones
extern char* IP_KERNEL_MEMORY;
extern char* PUERTO_KERNEL_MEMORY;
extern char* PUERTO_ESCUCHA;

//------------------------------------------
// FUNCIONES


t_config* iniciar_config(char* path_config);
void cargar_configuraciones(t_config* config);
void iniciar_logger();
void loggear_config();

#endif
