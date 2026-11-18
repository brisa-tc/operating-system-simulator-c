#ifndef CONEXION_H
#define CONEXION_H

#include "utils/comunicacion.h"
#include "utils/lecturaEscritura.h"
#include <commons/string.h>
#include <pthread.h>

extern void *memoria;
extern int tamanio_memoria;
extern pthread_mutex_t mutex_memoria;

bool validar_acceso(int direccion, int tamanio, int tamanio_memoria);
void escribir_memoria_stick(void* memoria, int direccion, const void* contenido, int tamanio, pthread_mutex_t* mutex);
void leer_memoria_stick(void* memoria, int direccion, void* contenido, int tamanio, pthread_mutex_t* mutex);

void* esperar_kernel_memory(void* socket_kernel_memory);
void esperar_cpus(int socket_server);
void* atender_cpu(void* socket_cliente);
void atender_cliente(int socket_cliente);

#endif
