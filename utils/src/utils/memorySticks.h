#ifndef MEMORY_STICKS_H
#define MEMORY_STICKS_H

#include "comunicacion.h"
#include <commons/collections/list.h>

typedef struct
{
    int tamanio; 
    char *ip; 
    char *puerto;
    int socket;
    uint32_t base;
} t_memoryStick_info;

// envio individual
/**
 * @brief Serializa y envía una única estructura t_memoryStick_info.
 * @param info Puntero a la estructura con los datos.
 * @param socket_destino Socket hacia donde se envía.
 */
void enviar_memory_stick_info(t_memoryStick_info* info, int socket_destino);

/**
 * @brief Recibe y deserializa una única estructura t_memoryStick_info desde el socket.
 * @param socket_origen Socket de donde provienen los datos.
 * @return Puntero a t_memoryStick_info creado en el heap.
 */
t_memoryStick_info* recibir_memory_stick_info(int socket_origen);

//envio de lista
/**
 * @brief Serializa y envía una t_list de estructuras t_memoryStick_info.
 * @param lista Puntero a la t_list de las commons.
 * @param socket_destino Socket hacia donde se envía la lista.
 */
void enviar_lista_memory_sticks(t_list* lista, int socket_destino);

/**
 * @brief Recibe un stream de red y reconstruye la t_list de t_memoryStick_info.
 * @param socket_origen Socket desde donde se reciben los datos.
 * @return Puntero a una nueva t_list cargada en memoria.
 */
t_list* recibir_lista_memory_sticks(int socket_origen);

/**
 * @brief Libera la memoria de una única estructura t_memoryStick_info.
 */
void eliminar_memory_stick_info(t_memoryStick_info* info);

/**
 * @brief Libera por completo la t_list y todos los t_memoryStick_info contenidos en ella.
 */
void eliminar_lista_memory_sticks(t_list* lista);
#endif
