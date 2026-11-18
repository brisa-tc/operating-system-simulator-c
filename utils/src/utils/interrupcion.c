
#include "interrupcion.h"


void enviar_interrupcion(int pid, bool esDesalojo, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = INTERRUPCION_CPU;
    paquete->buffer = malloc(sizeof(t_buffer));

    // El tamaño es fijo: un int y un bool
    paquete->buffer->size = sizeof(int) + sizeof(bool);
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;
    // 1. Serializar PID
    memcpy(paquete->buffer->stream + offset, &pid, sizeof(int));
    offset += sizeof(int);

    // 2. Serializar bandera de desalojo
    memcpy(paquete->buffer->stream + offset, &esDesalojo, sizeof(bool));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


void recibir_interrupcion(int socket_origen, int* out_pid, bool* out_esDesalojo) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    int offset = 0;
    // 1. Extraer PID
    memcpy(out_pid, buffer + offset, sizeof(int));
    offset += sizeof(int);

    // 2. Extraer bandera
    memcpy(out_esDesalojo, buffer + offset, sizeof(bool));

    free(buffer);
}

void enviar_cpu_desalojada(int pid_desalojado, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = CPU_DESALOJADA;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, &pid_desalojado, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

int recibir_cpu_desalojada(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    int pid_desalojado;
    memcpy(&pid_desalojado, buffer, sizeof(int));

    free(buffer);
    return pid_desalojado;
}

void enviar_error_cpu(int pid, int socket_destino) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CPU_ERROR;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete(paquete, socket_destino);
    eliminar_paquete(paquete);
}

void enviar_seg_fault_cpu(int pid, int socket_destino) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CPU_SEG_FAULT;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete(paquete, socket_destino);
    eliminar_paquete(paquete);
}

int recibir_error_cpu(int socket_origen) {
    t_list* valores = recibir_paquete(socket_origen);
    int pid = -1;

    if (valores != NULL && !list_is_empty(valores))
        pid = *(int*)list_get(valores, 0);

    if (valores != NULL)
        list_destroy_and_destroy_elements(valores, free);

    return pid;
}
