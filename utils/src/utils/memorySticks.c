#include "memorySticks.h"


void enviar_memory_stick_info(t_memoryStick_info* info, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = MEMORY_STICKS;
    paquete->buffer = malloc(sizeof(t_buffer));

    int len_ip = strlen(info->ip) + 1;
    int len_puerto = strlen(info->puerto) + 1;

    // Tamaño total: tamanio (int) + socket (int) + base (uint32_t) + len_ip (int) + ip + len_puerto (int) + puerto
    paquete->buffer->size = sizeof(int) * 4 + sizeof(uint32_t) + len_ip + len_puerto;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    memcpy(paquete->buffer->stream + offset, &(info->tamanio), sizeof(int));
    offset += sizeof(int);

    memcpy(paquete->buffer->stream + offset, &(info->socket), sizeof(int));
    offset += sizeof(int);

    memcpy(paquete->buffer->stream + offset, &(info->base), sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + offset, &len_ip, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, info->ip, len_ip);
    offset += len_ip;

    memcpy(paquete->buffer->stream + offset, &len_puerto, sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, info->puerto, len_puerto);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_memoryStick_info* recibir_memory_stick_info(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_memoryStick_info* info = malloc(sizeof(t_memoryStick_info));
    int offset = 0;
    int len_ip, len_puerto;

    // Extraer tamanio
    memcpy(&(info->tamanio), buffer + offset, sizeof(int));
    offset += sizeof(int);

    // Extraer socket
    memcpy(&(info->socket), buffer + offset, sizeof(int));
    offset += sizeof(int);

    // Extraer base
    memcpy(&(info->base), buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Extraer IP
    memcpy(&len_ip, buffer + offset, sizeof(int));
    offset += sizeof(int);
    info->ip = malloc(len_ip);
    memcpy(info->ip, buffer + offset, len_ip);
    offset += len_ip;

    // Extraer Puerto
    memcpy(&len_puerto, buffer + offset, sizeof(int));
    offset += sizeof(int);
    info->puerto = malloc(len_puerto);
    memcpy(info->puerto, buffer + offset, len_puerto);

    free(buffer);
    return info;
}


void enviar_lista_memory_sticks(t_list* lista, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = LIST_MEMORY_STICKS;
    paquete->buffer = malloc(sizeof(t_buffer));

    int tamaño_stream = sizeof(int); // Para guardar la cantidad de elementos de la lista
    
    for (int i = 0; i < list_size(lista); i++) {
        t_memoryStick_info* info = list_get(lista, i);
        tamaño_stream += sizeof(int) * 4 + sizeof(uint32_t) + (strlen(info->ip) + 1) + (strlen(info->puerto) + 1);
    }

    paquete->buffer->size = tamaño_stream;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    int cantidad_elementos = list_size(lista);
    
    // 1. Guardamos la cantidad de elementos al inicio del stream
    memcpy(paquete->buffer->stream + offset, &cantidad_elementos, sizeof(int));
    offset += sizeof(int);

    // 2. Serializamos secuencialmente cada elemento
    for (int i = 0; i < cantidad_elementos; i++) {
        t_memoryStick_info* info = list_get(lista, i);
        int len_ip = strlen(info->ip) + 1;
        int len_puerto = strlen(info->puerto) + 1;

        memcpy(paquete->buffer->stream + offset, &(info->tamanio), sizeof(int));
        offset += sizeof(int);

        memcpy(paquete->buffer->stream + offset, &(info->socket), sizeof(int));
        offset += sizeof(int);

        memcpy(paquete->buffer->stream + offset, &(info->base), sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(paquete->buffer->stream + offset, &len_ip, sizeof(int));
        offset += sizeof(int);
        memcpy(paquete->buffer->stream + offset, info->ip, len_ip);
        offset += len_ip;

        memcpy(paquete->buffer->stream + offset, &len_puerto, sizeof(int));
        offset += sizeof(int);
        memcpy(paquete->buffer->stream + offset, info->puerto, len_puerto);
        offset += len_puerto;
    }

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_list* recibir_lista_memory_sticks(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    t_list* lista = list_create();
    int offset = 0;
    int cantidad_elementos;

    // 1. Leer la cantidad de elementos que vienen en el flujo
    memcpy(&cantidad_elementos, buffer + offset, sizeof(int));
    offset += sizeof(int);

    // 2. Reconstruir e insertar cada elemento en la lista
    for (int i = 0; i < cantidad_elementos; i++) {
        t_memoryStick_info* info = malloc(sizeof(t_memoryStick_info));
        int len_ip, len_puerto;

        memcpy(&(info->tamanio), buffer + offset, sizeof(int));
        offset += sizeof(int);

        memcpy(&(info->socket), buffer + offset, sizeof(int));
        offset += sizeof(int);

        memcpy(&(info->base), buffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(&len_ip, buffer + offset, sizeof(int));
        offset += sizeof(int);
        info->ip = malloc(len_ip);
        memcpy(info->ip, buffer + offset, len_ip);
        offset += len_ip;

        memcpy(&len_puerto, buffer + offset, sizeof(int));
        offset += sizeof(int);
        info->puerto = malloc(len_puerto);
        memcpy(info->puerto, buffer + offset, len_puerto);
        offset += len_puerto;

        info->socket = -1;

        list_add(lista, info);
    }

    free(buffer);
    return lista;
}

void eliminar_memory_stick_info(t_memoryStick_info* info) {
    if (info != NULL) {
        free(info->ip);
        free(info->puerto);
        free(info);
    }
}

void eliminar_lista_memory_sticks(t_list* lista) {
    if (lista != NULL) {
        // list_destroy_and_destroy_elements aplica la función pasándole cada elemento
        list_destroy_and_destroy_elements(lista, (void*)eliminar_memory_stick_info);
    }
}
