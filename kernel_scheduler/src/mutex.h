#ifndef MUTEX_H
#define MUTEX_H
#include <pthread.h>
#include "types.h"

void inicializar_mutexes();


bool mutex_lock(t_pcb* pcb, char* nombre);

void mutex_unlock(t_pcb* pcb, char* nombre);

t_mutex_kernel* buscar_mutex(char* nombre);

void mutex_create(char* nombre);

void liberar_mutexes_por_proceso(t_pcb* pcb);

#endif
