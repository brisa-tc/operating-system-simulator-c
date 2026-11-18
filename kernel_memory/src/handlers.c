#include "handlers.h"
#include <string.h>

void atender_create_process(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid    = *(int*)list_get(valores, 0);
    char* path =  (char*)list_get(valores, 1);


    bool ok = crear_proceso(pid, path);
    if (!ok) {
    enviar_codigo(MEM_ERROR, socket_cliente);
    list_destroy_and_destroy_elements(valores, free);
    return;
    }
    enviar_codigo(MEM_OK, socket_cliente);
    list_destroy_and_destroy_elements(valores, free);



    // t_contexto* ctx = crear_contexto(pid, path);

    // if(ctx == NULL)
    // {
    //     log_warning(logger, "## PID: %d - Error al crear proceso", pid);
    //     enviar_codigo(MEM_ERROR, socket_cliente);
    //     list_destroy_and_destroy_elements(valores, free);
    //     return;
    // }
    // log_info(logger, "## PID: %d - Proceso Creado", pid);
    // enviar_codigo(MEM_OK, socket_cliente);

    // list_destroy_and_destroy_elements(valores, free);
}

void atender_finish_process(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(valores, 0);

    finalizar_proceso(pid);

    enviar_codigo(MEM_OK, socket_cliente);

    list_destroy_and_destroy_elements(valores, free);
}

void atender_suspend_process(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(valores, 0);

    bool ok = suspender_proceso(pid);

    enviar_codigo(ok ? MEM_OK : MEM_ERROR, socket_cliente);

    list_destroy_and_destroy_elements(valores, free);
}

void atender_desuspend_process(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(valores, 0);

    bool ok = des_suspender_proceso(pid);

    enviar_codigo(ok ? MEM_OK : MEM_ERROR, socket_cliente);

    list_destroy_and_destroy_elements(valores, free);
}


void atender_create_segment(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid     = *(int*)list_get(valores, 0);
    int id_seg  = *(int*)list_get(valores, 1);
    int tamanio = *(int*)list_get(valores, 2);

    list_destroy_and_destroy_elements(valores, free);

    t_contexto* ctx = buscar_contexto(pid);
    if(ctx == NULL)
    {
        log_warning(logger, "## PID: %d - CREATE_SEGMENT - Contexto no encontrado", pid);
        enviar_codigo(MEM_ERROR, socket_cliente);
        return;
    }

    if (tamanio <= 0 || tamanio > segment_max_size) {
        log_warning(logger,
                    "## PID: %d - Segmento %d rechazado - Tamaño: %d - Max Size: %d",
                    pid,
                    id_seg,
                    tamanio,
                    segment_max_size);
        enviar_codigo(MEM_ERROR, socket_cliente);
        return;
    }

    t_segmento* seg = crear_segmento(ctx, id_seg, tamanio);

    if(seg != NULL)
    {
        enviar_codigo(MEM_OK, socket_cliente);
        return;
    }

    // No hubo espacio — ver si hay espacio total pero fragmentado
    int espacio_libre = obtener_espacio_libre();
    if(espacio_libre >= tamanio)
    {
        log_info(logger, "## Inicio de compactación");
        enviar_codigo(MEM_COMPACTACION, socket_cliente);
    }
    else
    {
        enviar_codigo(MEM_ERROR, socket_cliente);
    }
}

void atender_delete_segment(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid    = *(int*)list_get(valores, 0);
    int id_seg = *(int*)list_get(valores, 1);

    list_destroy_and_destroy_elements(valores, free);

    t_contexto* ctx = buscar_contexto(pid);
    if(ctx == NULL)
    {
        log_warning(logger, "## PID: %d - DELETE_SEGMENT - Contexto no encontrado", pid);
        enviar_codigo(MEM_ERROR, socket_cliente);
        return;
    }

    eliminar_segmento(ctx, id_seg);
    enviar_codigo(MEM_OK, socket_cliente);
}

void atender_mem_write(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(valores, 0);
    uint32_t dir_logica = *(uint32_t*)list_get(valores, 1);
    uint32_t tamanio = *(uint32_t*)list_get(valores, 2);
    char* contenido = list_get(valores, 3);

    bool ok = escribir_datos(pid, dir_logica, (int)tamanio, contenido);

    enviar_codigo(ok ? MEM_OK : MEM_ERROR, socket_cliente);

    list_destroy_and_destroy_elements(valores, free);
}

void atender_mem_read(int socket_cliente)
{
    t_list* valores = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(valores, 0);
    uint32_t dir_logica = *(uint32_t*)list_get(valores, 1);
    uint32_t tamanio = *(uint32_t*)list_get(valores, 2);

    char* contenido = leer_datos(pid, dir_logica, (int)tamanio);

    if (contenido == NULL) {
        enviar_codigo(MEM_ERROR, socket_cliente);
    } else {
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = MEM_READ;
        agregar_a_paquete(paquete, contenido, strlen(contenido) + 1);
        enviar_paquete(paquete, socket_cliente);
        eliminar_paquete(paquete);
        free(contenido);
    }

    list_destroy_and_destroy_elements(valores, free);
}
