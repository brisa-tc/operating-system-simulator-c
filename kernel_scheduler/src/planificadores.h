#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

#include <stdbool.h>
#include <commons/collections/list.h>
#include "types.h"

void inicializar_algoritmos_planificacion(void);

void* planificador_largo_plazo(void* arg);
void* planificador_mediano_plazo(void* arg);

bool comparar_prioridad_y_tiempo(void* a, void* b);

void vaciar_lista(t_list* lista);
void finalizar_todos_los_procesos(void);
void liberar_memoria(t_pcb* pcb);
void pasar_a_exit(t_pcb* pcb);
void pasar_a_exit_con_motivo(t_pcb* pcb, const char* motivo);

#endif
