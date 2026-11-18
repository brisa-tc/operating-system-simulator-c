#ifndef SEGMENTOS_H
#define SEGMENTOS_H

#include "estructura.h"
#include <stdint.h>

t_segmento* crear_segmento(t_contexto* ctx, int id, int tamanio);
void        eliminar_segmento(t_contexto* ctx, int id_segmento);
t_hueco*    buscar_best_fit(int tamanio);
t_hueco*    buscar_worst_fit(int tamanio);
uint32_t    obtener_memoria_total();

#endif
