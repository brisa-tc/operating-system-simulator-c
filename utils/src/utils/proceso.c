#include "proceso.h"


int recibir_respuesta_memoria(int socket) {
    return recibir_operacion(socket);
}

void enviar_crear_proceso(int socket, int pid, char* path) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CREATE_PROCESS;

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, path, strlen(path) + 1);

    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void enviar_finalizar_proceso(int socket, int pid) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = FINISH_PROCESS;

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void enviar_suspender_proceso(int socket, int pid) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SUSPEND_PROCESS;

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void enviar_desuspender_proceso(int socket, int pid) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = DESUSPEND_PROCESS;

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}