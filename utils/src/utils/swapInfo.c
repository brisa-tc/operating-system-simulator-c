#include "swapInfo.h"

void enviar_swap_block_info(int block_size, int swap_file_size, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = SWAP_BLOCK;
    paquete->buffer = malloc(sizeof(t_buffer));

    // El tamaño del buffer son dos enteros fijos
    paquete->buffer->size = sizeof(int) * 2;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    // 1. Serializar el tamaño del bloque
    memcpy(paquete->buffer->stream + offset, &block_size, sizeof(int));
    offset += sizeof(int);

    // 2. Serializar el tamaño del archivo
    memcpy(paquete->buffer->stream + offset, &swap_file_size, sizeof(int));

    // Calculamos los bytes totales para tu función de serialización original
    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

void recibir_swap_block_info(int socket_origen, int* out_block_size, int* out_swap_file_size) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    int offset = 0;
    // 1. Extraer block_size
    memcpy(out_block_size, buffer + offset, sizeof(int));
    offset += sizeof(int);

    // 2. Extraer swap_file_size
    memcpy(out_swap_file_size, buffer + offset, sizeof(int));

    free(buffer);
}