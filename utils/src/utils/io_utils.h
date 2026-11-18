#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include "comunicacion.h"

/* ==========================================================================
   1. OPERACIÓN: IO_SLEEP
   ========================================================================== */

/**
 * @brief [PEDIDO] Serializa y envía una petición de bloqueo por tiempo (Sleep) hacia la interfaz.
 * * @param pid             Identificador del proceso (PCB) que solicita la operación.
 * @param tiempo          Cantidad de unidades de tiempo/milisegundos que el proceso debe esperar.
 * @param socket_destino  Descriptor del socket conectado al módulo de I/O.
 */
void enviar_pedido_io_sleep(int pid, int tiempo, int socket_destino);

/**
 * @brief [PEDIDO] Recibe y deserializa los parámetros de una petición de IO_SLEEP.
 * * @param socket_origen   Descriptor del socket de donde provienen los datos.
 * @param out_pid         [Salida] Puntero donde se almacenará el PID del proceso emisor.
 * @param out_tiempo      [Salida] Puntero donde se almacenará el tiempo de bloqueo.
 */
void recibir_pedido_io_sleep(int socket_origen, int* out_pid, int* out_tiempo);


/* ==========================================================================
   2. OPERACIÓN: IO_STDIN
   ========================================================================== */

/**
 * @brief [PEDIDO] Serializa y envía una petición de lectura por teclado (STDIN).
 * * @param pid                 Identificador del proceso que solicita la lectura.
 * @param cantidadCaracteres  Cantidad máxima de bytes/caracteres que se esperan leer.
 * @param socket_destino      Descriptor del socket conectado al módulo de I/O.
 */
void enviar_pedido_io_stdin(int pid, int cantidadCaracteres, int socket_destino);

/**
 * @brief [PEDIDO] Recibe y deserializa los parámetros de una petición de IO_STDIN.
 * * @param socket_origen   Descriptor del socket de donde provienen los datos.
 * @param out_pid         [Salida] Puntero donde se almacenará el PID del proceso.
 * @param out_cantidad    [Salida] Puntero donde se almacenará la cantidad de caracteres solicitados.
 */
void recibir_pedido_io_stdin(int socket_origen, int* out_pid, int* out_cantidad);

/**
 * @brief [RESPUESTA] Envía hacia el solicitante la cadena de caracteres leída desde la interfaz STDIN.
 * * @param contenidoLeido  Cadena de texto (string) ingresada por el usuario. Debe incluir el '\0'.
 * @param socket_destino  Descriptor del socket de red de destino.
 */
void enviar_respuesta_io_stdin(char* contenidoLeido, int socket_destino);

/**
 * @brief [RESPUESTA] Recibe y deserializa el flujo de texto enviado como respuesta a un STDIN.
 * * @warning Aloca memoria dinámica para el string retornado. El llamador debe aplicar free().
 * * @param socket_origen   Descriptor del socket de red de origen.
 * @return char* Puntero a la cadena de caracteres recibida en el heap.
 */
char* recibir_respuesta_io_stdin(int socket_origen);


/* ==========================================================================
   3. OPERACIÓN: IO_STDOUT
   ========================================================================== */

/**
 * @brief [PEDIDO] Serializa y envía una petición para mostrar texto en pantalla (STDOUT).
 * * @param pid             Identificador del proceso que solicita la escritura.
 * @param contenido       Cadena de caracteres (string) que se desea imprimir en pantalla.
 * @param socket_destino  Descriptor del socket conectado al módulo de I/O.
 */
void enviar_pedido_io_stdout(int pid, char* contenido, int socket_destino);

/**
 * @brief [PEDIDO] Recibe y deserializa una petición de IO_STDOUT que contiene texto variable.
 * * @warning Aloca memoria dinámica para (*out_contenido). Se debe liberar con free() tras su uso.
 * * @param socket_origen   Descriptor del socket de donde provienen los datos.
 * @param out_pid         [Salida] Puntero donde se guardará el PID del proceso.
 * @param out_contenido   [Salida] Dirección de un puntero (char**) donde se guardará el texto en el heap.
 */
void recibir_pedido_io_stdout(int socket_origen, int* out_pid, char** out_contenido);


/* ==========================================================================
   4. RESPUESTAS GENÉRICAS (SOPORTE PARA BOOLEANOS)
   ========================================================================== */

/**
 * @brief [RESPUESTA] Envía un estado booleano de éxito/error para operaciones sin retorno complejo (SLEEP y STDOUT).
 * * @param opcode          Código de operación correspondiente a la respuesta (IO_SLEEP o IO_STDOUT).
 * @param resultado       true si la operación fue exitosa, false en caso contrario.
 * @param socket_destino  Descriptor del socket de red de destino.
 */
void enviar_respuesta_io_generica(op_code opcode, bool resultado, int socket_destino);

/**
 * @brief [RESPUESTA] Recibe el estado booleano de confirmación de una operación de I/O genérica.
 * * @param socket_origen   Descriptor del socket de red de origen.
 * @return true           La interfaz completó la tarea exitosamente.
 * @return false          Ocurrió un error en la interfaz.
 */
bool recibir_respuesta_io_generica(int socket_origen);

#endif /* IO_UTILS_H_ */