#ifndef CPU_CONEXIONES_H
#define CPU_CONEXIONES_H

#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <utils/comunicacion.h>
#include <utils/identificadores.h>
#include <utils/lecturaEscritura.h>
#include <utils/interrupcion.h>
#include <utils/instruccion.h>
#include <utils/memorySticks.h>
#include <stdbool.h>
#include "ciclo_instruccion.h"

extern t_list* lista_memory_sticks;
extern pthread_mutex_t mutex_memory_sticks;
extern t_list* tabla_segmentos_actual;
extern pthread_mutex_t mutex_tabla_segmentos;
extern int tam_max_segmento_cpu;

void* atender_servidor(void* arg);
void* esperar_respuesta(int socket_servidor);
   

#endif
