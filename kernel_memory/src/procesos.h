#ifndef PROCESOS_H
#define PROCESOS_H

#include "estructura.h"
#include <stdbool.h>
#include <stdint.h>

t_contexto* crear_contexto(int pid, char* path);
t_contexto* buscar_contexto(int pid);

bool crear_proceso(int pid, char* path);
void finalizar_proceso(int pid);
bool suspender_proceso(int pid);
bool des_suspender_proceso(int pid);

int      traducir_direccion(int pid, uint32_t dir_logica, int tamanio, uint32_t* dir_fisica);
char*    leer_datos(int pid, uint32_t dir_logica, int tamanio);
bool     escribir_datos(int pid, uint32_t dir_logica, int tamanio, char* contenido);
int      obtener_espacio_libre();
uint32_t ampliar_memoria_usuario(uint32_t tamanio);
bool     compactar_memoria(void);

#endif
