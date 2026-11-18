#ifndef PROCESO_H_
#define PROCESO_H_
#include "comunicacion.h"


// Scheduler → Memory
void enviar_crear_proceso(int socket, int pid, char* path);

void enviar_finalizar_proceso(int socket, int pid);

void enviar_suspender_proceso(int socket, int pid);

void enviar_desuspender_proceso(int socket, int pid);

// Utils
int recibir_respuesta_memoria(int socket);

#endif