#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <utils/comunicacion.h>
#include <utils/identificadores.h>
#include <utils/lecturaEscritura.h>
#include <utils/swapInfo.h>
#include "swap.h"

t_log *logger;

static void atender_swap_write_block(int socket_kernel_memory, int swap_fd, int block_size, int block_count) {
    t_list* valores = recibir_paquete(socket_kernel_memory);

    int bloque = *(int*)list_get(valores, 0);
    int tamanio = *(int*)list_get(valores, 1);
    char* contenido = list_get(valores, 2);

    if (!bloque_valido(bloque, block_count) || tamanio != block_size) {
        enviar_codigo(MEM_ERROR, socket_kernel_memory);
        list_destroy_and_destroy_elements(valores, free);
        return;
    }

    off_t offset = bloque * block_size;
    lseek(swap_fd, offset, SEEK_SET);

    ssize_t escritos = write(swap_fd, contenido, block_size);
    fsync(swap_fd);

    log_info(logger, "## Escritura del bloque: %d", bloque);

    enviar_codigo(escritos == block_size ? MEM_OK : MEM_ERROR, socket_kernel_memory);
    list_destroy_and_destroy_elements(valores, free);
}

static void atender_swap_read_block(int socket_kernel_memory, int swap_fd, int block_size, int block_count) {
    t_list* valores = recibir_paquete(socket_kernel_memory);
    int bloque = *(int*)list_get(valores, 0);
    list_destroy_and_destroy_elements(valores, free);

    if (!bloque_valido(bloque, block_count)) {
        enviar_codigo(MEM_ERROR, socket_kernel_memory);
        return;
    }

    char* contenido = calloc(block_size, sizeof(char));
    if (contenido == NULL) {
        enviar_codigo(MEM_ERROR, socket_kernel_memory);
        return;
    }

    off_t offset = bloque * block_size;
    lseek(swap_fd, offset, SEEK_SET);
    ssize_t leidos = read(swap_fd, contenido, block_size);

    if (leidos < 0) {
        free(contenido);
        enviar_codigo(MEM_ERROR, socket_kernel_memory);
        return;
    }

    log_info(logger, "## Lectura del bloque: %d", bloque);

    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SWAP_READ_BLOCK;
    agregar_a_paquete(paquete, contenido, block_size);
    enviar_paquete(paquete, socket_kernel_memory);
    eliminar_paquete(paquete);
    free(contenido);
}

int main(int argc, char* argv[]) {
    //Inicio de modulo
    t_config *config = config_create(argv[1]);
    char* log_level = config_get_string_value(config, "LOG_LEVEL");
    char* swap_file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    int swap_file_size = config_get_int_value(config, "SWAP_FILE_SIZE");
    int block_size = config_get_int_value(config, "BLOCK_SIZE");
    char *ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    logger = log_create("log_swap.log", "swap", true, log_level_from_string(log_level));

    //logear datos del config
    log_debug(logger, "LOG LEVEL: %s",log_level);
    log_debug(logger, "SWAP FILE PATH: %s",swap_file_path);
    log_debug(logger, "SWAP FILE SIZE: %d",swap_file_size);
    log_debug(logger, "BLOCK SIZE: %d",block_size);
    log_debug(logger, "IP KERNEL MEMORY: %s", ip_kernel_memory);
    log_debug(logger, "PUERTO KERNEL MEMORY: %s", puerto_kernel_memory);

    //iniciar swap
    // Abrimos el archivo: crea si no existe, modo lectura/escritura
    int swap_fd = open(swap_file_path, O_RDWR | O_CREAT, 0644);
    
    if (swap_fd == -1) {
        perror("Error al abrir/crear el archivo de swap");
        return EXIT_FAILURE;
    }
    // Aseguramos que tenga el tamaño correcto 
    ftruncate(swap_fd, swap_file_size);

    int block_count = swap_file_size / block_size;

    // conectarse a kernel memory
    int socket_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);
    if (socket_kernel_memory == -1)
    {
        log_error(logger, "No se pudo conectar al kernel memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    //  Enviamos solo el código de operación para identificarnos
    enviar_identificador(MENSAJE_SWAP, socket_kernel_memory);
    enviar_swap_block_info(block_size, swap_file_size, socket_kernel_memory);
    log_info(logger, "## Conectado a Kernel Memory");

    while (1)
    {
        int op = recibir_operacion(socket_kernel_memory);

        if (op == -1)
        {
            log_debug(logger, "servidor desconectado");
            break;
        }

        switch (op)
        {
        case SWAP_WRITE_BLOCK:
            atender_swap_write_block(socket_kernel_memory, swap_fd, block_size, block_count);
            log_debug(logger, "SWAP_WRITE_BLOCK atendido");
            break;
        case SWAP_READ_BLOCK:
            atender_swap_read_block(socket_kernel_memory, swap_fd, block_size, block_count);
            log_debug(logger, "SWAP_READ_BLOCK atendido");
            break;
        case LECTURA:
            t_lectura *pedido_lectura = recibir_lectura(socket_kernel_memory);
            //leer en la direccion pedida
            t_lectura_respuesta *respuesta_lectura = malloc(sizeof(t_lectura_respuesta));
            respuesta_lectura->tamanio = pedido_lectura->tamanio;
            respuesta_lectura->contenido = calloc(respuesta_lectura->tamanio, 1);
            if(!direccion_swap_valida(pedido_lectura->direccion, pedido_lectura->tamanio, swap_file_size)){
                free(respuesta_lectura->contenido);
                respuesta_lectura->tamanio = 0;
                respuesta_lectura->contenido = NULL;
                enviar_respuesta_lectura(respuesta_lectura, socket_kernel_memory);
                eliminar_respuesta_lectura(respuesta_lectura);
                free(pedido_lectura);
                break;
            }
            // Nos movemos a la posición (direccion es el desplazamiento absoluto en bytes)
            lseek(swap_fd, pedido_lectura->direccion, SEEK_SET);

            // Leemos exactamente el tamaño de un bloque
            read(swap_fd, (void *)respuesta_lectura->contenido, pedido_lectura->tamanio);

            //enviar respuesta
            enviar_respuesta_lectura(respuesta_lectura, socket_kernel_memory);
            eliminar_respuesta_lectura(respuesta_lectura);
            free(pedido_lectura);
            log_debug(logger, "LECTURA atendida");
            break;
        case ESCRITURA:
            t_escritura *pedido_escritura = recibir_escritura(socket_kernel_memory);
            t_escritura_respuesta *respuesta_escritura = malloc(sizeof(t_escritura_respuesta));
            //escribir en la direccion pedida
            lseek(swap_fd, pedido_escritura->direccion, SEEK_SET);

            // Escribimos los bytes del bloque
            ssize_t result = write(swap_fd, pedido_escritura->contenido, pedido_escritura->tamanio);
            if (result == -1)
            {
                respuesta_escritura->resultado = false;
            }
            else
            {
                respuesta_escritura->resultado = true;
            }
            // enviar respuesta
            enviar_respuesta_escritura(respuesta_escritura, socket_kernel_memory);
            eliminar_escritura(pedido_escritura);
            free(respuesta_escritura);
            log_debug(logger, "ESCRITURA atendida");
            break;
        case MENSAJE:
            recibir_mensaje(socket_kernel_memory);
            break;
        default:
            log_warning(logger, "Operacion desconocida: %d", op);
            break;
        }
    }

    // liberar memoria
    close(swap_fd);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
