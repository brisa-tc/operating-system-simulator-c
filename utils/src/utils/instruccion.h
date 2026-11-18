#ifndef UTILS_INSTRUCCION_H
#define UTILS_INSTRUCCION_H

#include "comunicacion.h"
#include <stdbool.h>

/**
 * @brief Serializa y envía una instrucción en formato de cadena de caracteres.
 * @param instruccion Cadena de texto a enviar.
 * @param socket_destino Descriptor del socket destino.
 */
void enviar_instruccion(char* instruccion, int socket_destino);

/**
 * @brief Solicita a Kernel Memory la instrucción de un PID en un Program Counter.
 * @param pid Identificador del proceso.
 * @param pc Program Counter de la instrucción solicitada.
 * @param socket_destino Descriptor del socket de Kernel Memory.
 */
void solicitar_instruccion(int pid, int pc, int socket_destino);

/**
 * @brief Recibe una solicitud de instrucción enviada por CPU.
 * @param socket_origen Descriptor del socket origen.
 * @param pid Puntero donde guardar el PID recibido.
 * @param pc Puntero donde guardar el Program Counter recibido.
 * @return true si se recibieron PID y PC correctamente.
 */
bool recibir_solicitud_instruccion(int socket_origen, int* pid, int* pc);

/**
 * @brief Recibe y deserializa una instrucción de texto desde el socket.
 * @param socket_origen Descriptor del socket origen.
 * @return Puntero a la cadena de caracteres creada en el heap (requiere free).
 */
char* recibir_instruccion(int socket_origen);

/**
 * @brief Serializa y envía el valor del Program Counter.
 * @param pc Valor entero del Program Counter a enviar.
 * @param socket_destino Descriptor del socket destino.
 */
void enviar_program_counter(int pc, int socket_destino);

/**
 * @brief Recibe y deserializa el valor numérico del Program Counter.
 * @param socket_origen Descriptor del socket origen.
 * @return Valor entero del Program Counter recibido.
 */
int recibir_program_counter(int socket_origen);
#endif
