#ifndef CONTEXTO_UTILS_H_
#define CONTEXTO_UTILS_H_

#include "comunicacion.h"
#include <commons/collections/list.h>

typedef struct
{
    uint32_t PC;
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;
} t_registro;

typedef struct
{
    int id;
    uint32_t base;
    uint32_t tamanio;
} t_segmento_contexto;

/**
 * @brief Serializa y envía los registros del contexto de ejecución.
 * @param registros Puntero a la estructura con los registros a enviar.
 * @param socket_destino Descriptor del socket de destino.
 */
void enviar_contexto_ejecucion(t_registro* registros, int socket_destino);

/**
 * @brief Recibe y deserializa los registros del contexto de ejecución.
 * @param socket_origen Descriptor del socket de origen.
 * @return Puntero a t_registro creado en el heap (requiere free posterior).
 */
t_registro* recibir_contexto_ejecucion(int socket_origen);

/**
 * @brief Serializa y envía el segment_max_size (int).
 * @param segment_max_size Tamaño máximo de segmento a enviar.
 * @param socket_destino Descriptor del socket de destino.
 */
void enviar_segment_max_size(int segment_max_size, int socket_destino);

/**
 * @brief Recibe y deserializa el segment_max_size (int).
 * @param socket_origen Descriptor del socket de origen.
 * @return Valor entero del segment_max_size recibido.
 */
int recibir_segment_max_size(int socket_origen);

/**
 * @brief Serializa y envía una t_list de t_segmento_contexto.
 * @param tabla_segmentos Puntero a la t_list con los segmentos a enviar.
 * @param socket_destino Descriptor del socket de destino.
 */
void enviar_tabla_segmentos(t_list* tabla_segmentos, int socket_destino);

/**
 * @brief Recibe un stream de red y reconstruye la t_list de t_segmento_contexto.
 * @param socket_origen Socket desde donde se reciben los datos.
 * @return Puntero a una nueva t_list cargada en memoria.
 */
t_list* recibir_tabla_segmentos(int socket_origen);

/**
 * @brief Envía una solicitud de contexto de ejecución (solo PID).
 *        La CPU solicita a Memory el contexto para un proceso específico.
 * @param pid Identificador del proceso cuyo contexto se solicita.
 * @param socket_destino Descriptor del socket de destino (Memory).
 */
void enviar_pedido_contexto(int pid, int socket_destino);

/**
 * @brief Recibe una solicitud de contexto de ejecución y extrae el PID.
 * @param socket_origen Descriptor del socket de origen (CPU).
 * @return PID del proceso cuyo contexto fue solicitado.
 */
int recibir_pedido_contexto(int socket_origen);

#endif
