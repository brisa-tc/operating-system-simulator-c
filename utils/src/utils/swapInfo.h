#ifndef UTILS_SWAPINFO_H
#define UTILS_SWAPINFO_H

#include "comunicacion.h"

/**
 * @brief Serializa y envía la configuración de Swap (Tamaño de bloque y tamaño del archivo).
 * @param block_size Tamaño de cada bloque/página en Swap.
 * @param swap_file_size Tamaño total del archivo de Swap (en bloques o bytes según tu diseño).
 * @param socket_destino Descriptor del socket del módulo que recibirá la info.
 */
void enviar_swap_block_info(int block_size, int swap_file_size, int socket_destino);

/**
 * @brief Recibe y deserializa la información de configuración de Swap desde el socket.
 * @param socket_origen Descriptor del socket de origen.
 * @param out_block_size Puntero donde se almacenará el tamaño de bloque extraído.
 * @param out_swap_file_size Puntero donde se almacenará el tamaño del archivo extraído.
 */
void recibir_swap_block_info(int socket_origen, int* out_block_size, int* out_swap_file_size);

#endif