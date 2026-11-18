#include "lecturaEscritura.h"

void enviar_lectura(t_lectura* lectura, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = LECTURA;
    paquete->buffer = malloc(sizeof(t_buffer));
    
    paquete->buffer->size = sizeof(int) * 2;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &(lectura->direccion), sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, &(lectura->tamanio), sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_lectura* recibir_lectura(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_lectura* lectura = malloc(sizeof(t_lectura));
    int offset = 0;
    
    memcpy(&(lectura->direccion), buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&(lectura->tamanio), buffer + offset, sizeof(int));

    free(buffer);
    return lectura;
}

void enviar_respuesta_lectura(t_lectura_respuesta* respuesta, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = LECTURA; // Mismo op_code por enunciado
    paquete->buffer = malloc(sizeof(t_buffer));
    
    paquete->buffer->size = sizeof(int) + respuesta->tamanio;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    memcpy(paquete->buffer->stream, &respuesta->tamanio, sizeof(int));
    if (respuesta->tamanio > 0)
        memcpy(paquete->buffer->stream + sizeof(int), respuesta->contenido, respuesta->tamanio);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


t_lectura_respuesta* recibir_respuesta_lectura(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_lectura_respuesta* respuesta = malloc(sizeof(t_lectura_respuesta));
    memcpy(&respuesta->tamanio, buffer, sizeof(int));
    respuesta->contenido = malloc(respuesta->tamanio);
    if (respuesta->tamanio > 0)
        memcpy(respuesta->contenido, buffer + sizeof(int), respuesta->tamanio);

    free(buffer);
    return respuesta;
}

void enviar_escritura(t_escritura* escritura, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = ESCRITURA;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int) + sizeof(int) + escritura->tamanio;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    // 1. Copiar Dirección
    memcpy(paquete->buffer->stream + offset, &(escritura->direccion), sizeof(int));
    offset += sizeof(int);
    
    // 2. Copiar longitud de la cadena
    memcpy(paquete->buffer->stream + offset, &escritura->tamanio, sizeof(int));
    offset += sizeof(int);
    
    // 3. Copiar la cadena
    memcpy(paquete->buffer->stream + offset, escritura->contenido, escritura->tamanio);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_escritura* recibir_escritura(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_escritura* escritura = malloc(sizeof(t_escritura));
    int offset = 0;

    // 1. Extraer Dirección
    memcpy(&(escritura->direccion), buffer + offset, sizeof(int));
    offset += sizeof(int);

    // 2. Extraer longitud de la cadena
    memcpy(&escritura->tamanio, buffer + offset, sizeof(int));
    offset += sizeof(int);

    // 3. Extraer la cadena
    escritura->contenido = malloc(escritura->tamanio);
    memcpy(escritura->contenido, buffer + offset, escritura->tamanio);

    free(buffer);
    return escritura;
}

void enviar_respuesta_escritura(t_escritura_respuesta* respuesta, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = ESCRITURA; // Mismo op_code por enunciado
    paquete->buffer = malloc(sizeof(t_buffer));
    
    paquete->buffer->size = sizeof(bool);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    
    memcpy(paquete->buffer->stream, &(respuesta->resultado), sizeof(bool));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


t_escritura_respuesta* recibir_respuesta_escritura(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_escritura_respuesta* respuesta = malloc(sizeof(t_escritura_respuesta));
    memcpy(&(respuesta->resultado), buffer, sizeof(bool));

    free(buffer);
    return respuesta;
}

void eliminar_escritura(t_escritura* esc) {
    if (esc != NULL) {
        free(esc->contenido);
        free(esc);
    }
}

void eliminar_respuesta_lectura(t_lectura_respuesta* res) {
    if (res != NULL) {
        free(res->contenido);
        free(res);
    }
}
