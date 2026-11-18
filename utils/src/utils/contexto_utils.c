
#include "contexto_utils.h"

void enviar_contexto_ejecucion(t_registro* registros, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = CONTEXTO_EJEC;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(t_registro);
    paquete->buffer->stream = malloc(paquete->buffer->size);

    memcpy(paquete->buffer->stream, registros, sizeof(t_registro));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);

    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_registro* recibir_contexto_ejecucion(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);

    t_registro* registros = malloc(sizeof(t_registro));
    memcpy(registros, buffer, sizeof(t_registro));

    free(buffer);
    return registros;
}

void enviar_segment_max_size(int segment_max_size, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = SEGMENT_MAX_SIZE;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, &segment_max_size, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

int recibir_segment_max_size(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    int segment_max_size;
    memcpy(&segment_max_size, buffer, sizeof(int));
    free(buffer);
    return segment_max_size;
}

void enviar_tabla_segmentos(t_list* tabla_segmentos, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = TABLA_SEGMENTOS;
    paquete->buffer = malloc(sizeof(t_buffer));

    int cantidad = tabla_segmentos != NULL ? list_size(tabla_segmentos) : 0;
    paquete->buffer->size = sizeof(int) + cantidad * sizeof(t_segmento_contexto);
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &cantidad, sizeof(int));
    offset += sizeof(int);

    for (int i = 0; i < cantidad; i++) {
        t_segmento_contexto* segmento = list_get(tabla_segmentos, i);
        memcpy(paquete->buffer->stream + offset, segmento, sizeof(t_segmento_contexto));
        offset += sizeof(t_segmento_contexto);
    }

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_list* recibir_tabla_segmentos(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);

    t_list* lista = list_create();
    int offset = 0;
    int cantidad;

    memcpy(&cantidad, buffer + offset, sizeof(int));
    offset += sizeof(int);

    for (int i = 0; i < cantidad; i++) {
        t_segmento_contexto* segmento = malloc(sizeof(t_segmento_contexto));
        memcpy(segmento, buffer + offset, sizeof(t_segmento_contexto));
        offset += sizeof(t_segmento_contexto);
        list_add(lista, segmento);
    }

    free(buffer);
    return lista;
}

void enviar_pedido_contexto(int pid, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = SOLICITUD_CONTEXTO;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = sizeof(int);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, &pid, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);
    free(a_enviar);
    eliminar_paquete(paquete);
}

int recibir_pedido_contexto(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    int pid;
    memcpy(&pid, buffer, sizeof(int));
    free(buffer);
    return pid;
}
