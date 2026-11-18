#include "conexion.h"

extern int memory_delay;

bool validar_acceso(int direccion, int tamanio, int tamanio_memoria) {
    return direccion >= 0 &&
           tamanio >= 0 &&
           direccion <= tamanio_memoria &&
           tamanio <= tamanio_memoria - direccion;
}

void escribir_memoria_stick(void* memoria, int direccion, const void* contenido, int tamanio, pthread_mutex_t* mutex) {
    pthread_mutex_lock(mutex);
    memcpy(memoria + direccion, contenido, tamanio);
    pthread_mutex_unlock(mutex);
}

void leer_memoria_stick(void* memoria, int direccion, void* contenido, int tamanio, pthread_mutex_t* mutex) {
    pthread_mutex_lock(mutex);
    memcpy(contenido, memoria + direccion, tamanio);
    pthread_mutex_unlock(mutex);
}

void* esperar_kernel_memory(void* arg){
    int socket_kernel_memory = *(int*)arg;
    atender_cliente(socket_kernel_memory);
    return NULL;
}

void esperar_cpus(int socket_server){
while (1)
    {
        pthread_t thread;
        int *socket_cpu = malloc(sizeof(int));
        *socket_cpu = esperar_cliente(socket_server);

        if (*socket_cpu == -1) {
            free(socket_cpu);
            continue;
        }

        pthread_create(&thread, NULL, atender_cpu, socket_cpu);
        pthread_detach(thread);
    }
}

void* atender_cpu(void* arg) {
    int socket_cpu = *(int*)arg;
    free(arg);

    int cod_op = recibir_operacion(socket_cpu);
    if (cod_op == MENSAJE_CPU) {
        int id_cpu = recibir_operacion(socket_cpu);
        log_info(logger, "## CPU %d Conectada", id_cpu);
    } else {
        log_warning(logger, "Se esperaba identificación de CPU, se recibió operación %d", cod_op);
    }

    atender_cliente(socket_cpu);
    return NULL;
}

void atender_cliente(int socket_cliente){
    while (1) {
        int cod_op = recibir_operacion(socket_cliente);
        usleep(memory_delay * 1000); // Convertir milisegundos a microsegundos
        switch (cod_op)
        {
    case MENSAJE:
        recibir_mensaje(socket_cliente);
        break;
    case ESCRITURA:
        t_escritura* escritura = recibir_escritura(socket_cliente);
        t_escritura_respuesta *respuesta_escritura = malloc(sizeof(t_escritura_respuesta));

        respuesta_escritura->resultado = validar_acceso(
            escritura->direccion, escritura->tamanio, tamanio_memoria);

        if (respuesta_escritura->resultado) {
            escribir_memoria_stick(memoria, escritura->direccion,
                escritura->contenido, escritura->tamanio, &mutex_memoria);
        }

        log_info(logger, "## Escritura de %d bytes", escritura->tamanio);

        enviar_respuesta_escritura(respuesta_escritura, socket_cliente);
        eliminar_escritura(escritura);
        free(respuesta_escritura);
        break;
    case LECTURA:
        t_lectura* lectura = recibir_lectura(socket_cliente);
        bool lectura_valida = validar_acceso(
            lectura->direccion, lectura->tamanio, tamanio_memoria);

        t_lectura_respuesta *respuesta_lectura = malloc(sizeof(t_lectura_respuesta));
        respuesta_lectura->tamanio = lectura_valida ? lectura->tamanio : 0;
        respuesta_lectura->contenido = malloc(respuesta_lectura->tamanio);

        if (lectura_valida) {
            leer_memoria_stick(memoria, lectura->direccion,
                respuesta_lectura->contenido, lectura->tamanio, &mutex_memoria);
        }

        log_info(logger, "## Lectura de %d bytes", lectura->tamanio);

        enviar_respuesta_lectura(respuesta_lectura, socket_cliente);

        eliminar_respuesta_lectura(respuesta_lectura);
        free(lectura);
        break;
    case DESCONEXION:
        log_error(logger, "el cliente se desconecto. Terminando servidor");
        return;
    default:
        log_warning(logger, "Operacion desconocida. No quieras meter la pata");
        break;
        }
    }
}
