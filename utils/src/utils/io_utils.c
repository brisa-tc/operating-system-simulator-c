
#include "io_utils.h"

void enviar_pedido_io_sleep(int pid, int tiempo, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = IO_SLEEP;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int) * 2;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &pid, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, &tiempo, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

void recibir_pedido_io_sleep(int socket_origen, int* out_pid, int* out_tiempo) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    int offset = 0;
    memcpy(out_pid, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(out_tiempo, buffer + offset, sizeof(int));
    free(buffer);
}

void enviar_pedido_io_stdout(int pid, char* contenido, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = IO_STDOUT;
    paquete->buffer = malloc(sizeof(t_buffer));

    int len_contenido = strlen(contenido) + 1;
    // Tamaño total: pid (int) + tamaño string (int) + string
    paquete->buffer->size = sizeof(int) * 2 + len_contenido;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &pid, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, &len_contenido, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, contenido, len_contenido);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


void recibir_pedido_io_stdout(int socket_origen, int* out_pid, char** out_contenido) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    int offset = 0;
    int len_contenido;

    memcpy(out_pid, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&len_contenido, buffer + offset, sizeof(int));
    offset += sizeof(int);

    *out_contenido = malloc(len_contenido);
    memcpy(*out_contenido, buffer + offset, len_contenido);

    free(buffer);
}


void enviar_respuesta_io_generica(op_code opcode, bool resultado, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = opcode;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(bool);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, &resultado, sizeof(bool));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


bool recibir_respuesta_io_generica(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    bool resultado;
    memcpy(&resultado, buffer, sizeof(bool));
    free(buffer);
    return resultado;
}

void enviar_pedido_io_stdin(int pid, int cantidadCaracteres, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = IO_STDIN;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int) * 2;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &pid, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, &cantidadCaracteres, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


void recibir_pedido_io_stdin(int socket_origen, int* out_pid, int* out_cantidad) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    int offset = 0;
    memcpy(out_pid, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(out_cantidad, buffer + offset, sizeof(int));
    free(buffer);
}


void enviar_respuesta_io_stdin(char* contenidoLeido, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = IO_STDIN;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = strlen(contenidoLeido) + 1;
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, contenidoLeido, paquete->buffer->size);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


char* recibir_respuesta_io_stdin(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    char* contenido = strdup((char*)buffer);
    free(buffer);
    return contenido;
}