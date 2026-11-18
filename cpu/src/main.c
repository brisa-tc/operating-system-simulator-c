#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <utils/comunicacion.h>
#include <utils/identificadores.h>
#include <utils/memorySticks.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "globals.h"
#include "ciclo_instruccion.h"
#include "conexiones.h"
#include "registros.h"


int identificador;

int main(int argc, char *argv[])
{
    if(argc < 3) {
        fprintf(stderr, "Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Inicio de modulo
    t_config *config = config_create(argv[1]);
    if(config == NULL) {
        fprintf(stderr, "No se pudo cargar el config %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    identificador = atoi(argv[2]);
    char *log_level = config_get_string_value(config, "LOG_LEVEL");

    char *ip_kernel_scheduler = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char *puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");

    char *ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    char *ip_memory_stick = config_get_string_value(config, "IP_MEMORY_STICK");
    char *puerto_memory_stick = config_get_string_value(config, "PUERTO_MEMORY_STICK");

    //logs especificos para los logs individuales
    char* log_filename = string_from_format("log_cpu_%d.log", identificador);
    logger = log_create(log_filename, "cpu", true, log_level_from_string(log_level));
    free(log_filename);
    registros = calloc(1, sizeof(t_registro));

    if(registros == NULL) {
        log_error(logger, "No se pudo reservar memoria para registros");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    // logear datos del config
    log_debug(logger, "LOG LEVEL: %s", log_level);

    log_debug(logger, "IP KERNEL SCHEDULER: %s", ip_kernel_scheduler);
    log_debug(logger, "PUERTO KERNEL SCHEDULER: %s", puerto_kernel_scheduler);

    log_debug(logger, "IP KERNEL MEMORY: %s", ip_kernel_memory);
    log_debug(logger, "PUERTO KERNEL MEMORY: %s", puerto_kernel_memory);

    log_debug(logger, "IP MEMORY STICK: %s", ip_memory_stick);
    log_debug(logger, "PUERTO MEMORY STICK: %s", puerto_memory_stick);

    // conectar a kernel memory
    socket_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);
    if (socket_kernel_memory == -1)
    {
        log_error(logger, "No se pudo conectar al kernel memory");
        free(registros);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    enviar_identificador(MENSAJE_CPU, socket_kernel_memory);
    enviar_identificador(identificador, socket_kernel_memory);
    log_debug(logger, "Se conecto a Kernel Memory");
 

    // conectar a kernel scheduler
    
    socket_kernel_scheduler = crear_conexion(ip_kernel_scheduler, puerto_kernel_scheduler);
    if (socket_kernel_scheduler == -1)
    {
        log_error(logger, "No se pudo conectar al kernel scheduler");
        close(socket_kernel_memory);
        free(registros);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    enviar_identificador(MENSAJE_CPU, socket_kernel_scheduler);
    enviar_identificador(identificador, socket_kernel_scheduler);

    log_debug(logger, "Se conecto a Kernel Scheduler");

    // Hilo dedicado para el ciclo de instrucción
    pthread_t hilo_ciclo;
    pthread_create(&hilo_ciclo, NULL, atender_ciclo, NULL);

    // La CPU atiende Kernel Scheduler en un hilo y Kernel Memory en el hilo principal
    pthread_t hilo_scheduler;
    pthread_create(&hilo_scheduler, NULL, atender_servidor, (void*)&socket_kernel_scheduler);
    atender_servidor((void*)&socket_kernel_memory);

    // liberar memoria
    if (lista_memory_sticks != NULL) {
        for (int i = 0; i < list_size(lista_memory_sticks); i++) {
            t_memoryStick_info* info = list_get(lista_memory_sticks, i);
            if (info->socket > 0) close(info->socket);
        }
        eliminar_lista_memory_sticks(lista_memory_sticks);
    }
    if (tabla_segmentos_actual != NULL) {
        list_destroy_and_destroy_elements(tabla_segmentos_actual, free);
    }
    if (lista_interrupciones != NULL) {
        list_destroy_and_destroy_elements(lista_interrupciones, free);
    }
    if (buffer_instruccion != NULL) free(buffer_instruccion);
    free(registros);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
