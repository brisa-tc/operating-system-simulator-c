#include "instruccion.h"


void enviar_instruccion(char* instruccion, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = INSTRUCCION;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = strlen(instruccion) + 1;
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, instruccion, paquete->buffer->size);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

void solicitar_instruccion(int pid, int pc, int socket_destino) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = INSTRUCCION;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &pc, sizeof(int));

    enviar_paquete(paquete, socket_destino);
    eliminar_paquete(paquete);
}

bool recibir_solicitud_instruccion(int socket_origen, int* pid, int* pc) {
    t_list* valores = recibir_paquete(socket_origen);

    if (valores == NULL || list_size(valores) < 2) {
        if (valores != NULL) {
            list_destroy_and_destroy_elements(valores, free);
        }
        return false;
    }

    *pid = *(int*)list_get(valores, 0);
    *pc = *(int*)list_get(valores, 1);

    list_destroy_and_destroy_elements(valores, free);
    return true;
}

char* recibir_instruccion(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    // Al haber enviado con strlen + 1, podemos usar strdup directamente
    char* instruccion = strdup((char*)buffer);

    free(buffer);
    return instruccion;
}

void enviar_program_counter(int pc, int socket_destino) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = INSTRUCCION;
    paquete->buffer = malloc(sizeof(t_buffer));

    paquete->buffer->size = sizeof(int);
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, &pc, sizeof(int));

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void* a_enviar = serializar_paquete(paquete, bytes);
    
    send(socket_destino, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

int recibir_program_counter(int socket_origen) {
    int size;
    void* buffer = recibir_buffer(&size, socket_origen);
    
    int pc;
    memcpy(&pc, buffer, sizeof(int));

    free(buffer);
    return pc;
}
