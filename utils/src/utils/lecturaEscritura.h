#ifndef LECTURAESCRITURA_H_
#define LECTURAESCRITURA_H_

#include "comunicacion.h"

typedef struct{
int direccion;
int tamanio;
}t_lectura;

typedef struct{
int direccion;
int tamanio;
void* contenido;
}t_escritura;

typedef struct{
int tamanio;
void* contenido;
}t_lectura_respuesta;

typedef struct {
bool resultado;
}t_escritura_respuesta;

/**
 * @brief Serializa y envía una estructura t_lectura.
 * @param lectura Puntero a la estructura con los datos de lectura.
 * @param socket_destino Socket hacia donde se envía la petición.
 */
void enviar_lectura(t_lectura* lectura, int socket_destino) ;

/**
 * @brief Recibe y deserializa una estructura t_lectura desde el socket.
 * @param socket_origen Socket de donde provienen los datos.
 * @return Puntero a t_lectura creado en el heap.
 */
t_lectura* recibir_lectura(int socket_origen) ;

/**
 * @brief Serializa y envía la respuesta de una lectura (t_lectura_respuesta).
 * @param respuesta Estructura con la cadena de caracteres leída.
 * @param socket_destino Socket hacia donde se envía la respuesta.
 */
void enviar_respuesta_lectura(t_lectura_respuesta* respuesta, int socket_destino) ;

/**
 * @brief Recibe y deserializa la respuesta de una lectura.
 * @param socket_origen Socket del servidor.
 * @return Puntero a t_lectura_respuesta creado en el heap.
 */
t_lectura_respuesta* recibir_respuesta_lectura(int socket_origen) ;

/**
 * @brief Serializa y envía una estructura t_escritura (dirección, tamaño y bytes).
 * @param escritura Puntero a la estructura con los datos a escribir.
 * @param socket_destino Socket hacia donde se envía la petición.
 */
void enviar_escritura(t_escritura* escritura, int socket_destino) ;

/**
 * @brief Recibe y deserializa una estructura t_escritura desde el socket.
 * @param socket_origen Socket de donde provienen los datos.
 * @return Puntero a t_escritura creado en el heap.
 */
t_escritura* recibir_escritura(int socket_origen) ;

/**
 * @brief Serializa y envía la respuesta de una escritura (t_escritura_respuesta).
 * @param respuesta Estructura con el resultado booleano.
 * @param socket_destino Socket hacia donde se envía la respuesta.
 */
void enviar_respuesta_escritura(t_escritura_respuesta* respuesta, int socket_destino) ;

/**
 * @brief Recibe y deserializa la respuesta de una escritura.
 * @param socket_origen Socket del servidor.
 * @return Puntero a t_escritura_respuesta creado en el heap.
 */
t_escritura_respuesta* recibir_respuesta_escritura(int socket_origen) ;

void eliminar_escritura(t_escritura* esc) ;

void eliminar_respuesta_lectura(t_lectura_respuesta* res) ;

#endif
