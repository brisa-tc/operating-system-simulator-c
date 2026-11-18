#ifndef MEMORY_CLIENTE_H
#define MEMORY_CLIENTE_H

#include <stdbool.h>
#include <utils/comunicacion.h>

extern int conexion_memoria;
extern bool memoria_ampliada_pendiente;

void conectar_con_memoria();

bool memoria_crear_proceso(int pid, char* path);

bool memoria_finalizar_proceso(int pid);

bool memoria_desuspender_proceso(int pid);
bool memoria_suspender_proceso(int pid);

op_code memoria_crear_segmento(int pid, int id_segmento, int tamanio);

bool memoria_eliminar_segmento(int pid, int id_segmento);

bool memoria_solicitar_compactacion(void);

bool memoria_escribir(int pid, uint32_t direccion_logica, uint32_t tamanio, char* contenido);
char* memoria_leer(int pid, uint32_t direccion_logica, uint32_t tamanio);

void manejar_ampliacion_memoria(void);

#endif
