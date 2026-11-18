#ifndef UTILS_INTERRUPCION_H
#define UTILS_INTERRUPCION_H

#include "comunicacion.h"

/**
 * @brief Serializa y envía una interrupción (PID + Motivo de desalojo).
 * @param pid Identificador del proceso a interrumpir.
 * @param esDesalojo Booleano que indica si el motivo es un desalojo (true) o fin de quantum/otro (false).
 * @param socket_destino Descriptor del socket de la CPU.
 */
void enviar_interrupcion(int pid, bool esDesalojo, int socket_destino);

/**
 * @brief Recibe y deserializa una interrupción desde el socket.
 * @param socket_origen Descriptor del socket origen.
 * @param out_pid Puntero donde se almacenará el PID extraído.
 * @param out_esDesalojo Puntero donde se almacenará el booleano extraído.
 */
void recibir_interrupcion(int socket_origen, int* out_pid, bool* out_esDesalojo);

/**
 * @brief Serializa y envía la confirmación de CPU desalojada con el PID correspondiente.
 * @param pid_desalojado Identificador del proceso que acaba de dejar la CPU.
 * @param socket_destino Descriptor del socket del Kernel.
 */
void enviar_cpu_desalojada(int pid_desalojado, int socket_destino);

/**
 * @brief Recibe y deserializa el PID del proceso desalojado de la CPU.
 * @param socket_origen Descriptor del socket origen.
 * @return Identificador numérico (PID) del proceso desalojado.
 */
int recibir_cpu_desalojada(int socket_origen);

/**
 * @brief Notifica al Scheduler que la CPU no pudo continuar el proceso actual.
 */
void enviar_error_cpu(int pid, int socket_destino);
void enviar_seg_fault_cpu(int pid, int socket_destino);

/**
 * @brief Recibe el PID informado por una CPU que encontro un error fatal.
 */
int recibir_error_cpu(int socket_origen);

#endif
