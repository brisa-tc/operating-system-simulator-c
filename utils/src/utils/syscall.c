#include "syscall.h"


t_syscall* crear_t_syscall(e_tipo_syscall tipo, t_buffer* buffer) {
    t_syscall* syscall = malloc(sizeof(t_syscall));
    
    if (syscall == NULL) {
        return NULL; // Manejo de error de memoria
    }

    syscall->tipo_syscall = tipo;
    syscall->buffer = buffer;

    return syscall;
}

t_syscall* preparar_syscall_mutex(e_tipo_syscall tipo, char* nombre_mutex) {
    // 1. Creamos el buffer para los parámetros
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    
    // Calculamos el tamaño incluyendo el centinela '\0'
    buffer_params->size = strlen(nombre_mutex) + 1; 
    buffer_params->stream = malloc(buffer_params->size);

    // 2. Copiamos el nombre al stream del buffer
    memcpy(buffer_params->stream, nombre_mutex, buffer_params->size);

    // 3. Creamos y retornamos la estructura t_syscall
    return crear_t_syscall(tipo, buffer_params);
}

t_syscall* preparar_syscall_mem_alloc(int id_segmento, int tamanio) {
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    
    // Tamaño de 2 enteros
    buffer_params->size = sizeof(int) * 2; 
    buffer_params->stream = malloc(buffer_params->size);

    int desplazamiento = 0;
    
    // 1. Copiamos el ID del segmento
    memcpy(buffer_params->stream + desplazamiento, &id_segmento, sizeof(int));
    desplazamiento += sizeof(int);

    // 2. Copiamos el tamaño
    memcpy(buffer_params->stream + desplazamiento, &tamanio, sizeof(int));

    return crear_t_syscall(SC_MEM_ALLOC, buffer_params);
}

t_syscall* preparar_syscall_mem_free(int id_segmento) {
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    
    buffer_params->size = sizeof(int); 
    buffer_params->stream = malloc(buffer_params->size);

    // Copiamos solo el ID
    memcpy(buffer_params->stream, &id_segmento, sizeof(int));

    return crear_t_syscall(SC_MEM_FREE, buffer_params);
}

t_syscall* preparar_syscall_sleep(int tiempo_ms) {
    // 1. Creamos el buffer interno para los parámetros
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    buffer_params->size = sizeof(int);
    buffer_params->stream = malloc(buffer_params->size);

    // 2. Serializamos el valor del tiempo en el stream
    memcpy(buffer_params->stream, &tiempo_ms, sizeof(int));

    // 3. Usamos la función genérica para armar la t_syscall
    return crear_t_syscall(SC_SLEEP, buffer_params);
}

t_syscall* preparar_syscall_io(e_tipo_syscall tipo, uint32_t direccion_logica, uint32_t tamanio) {
    
    // 1. Preparamos el buffer para los parámetros
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    buffer_params->size = sizeof(uint32_t) * 2; 
    buffer_params->stream = malloc(buffer_params->size);

    int desplazamiento = 0;

    // 2. Serializamos los valores directamente en el stream
    memcpy(buffer_params->stream + desplazamiento, &direccion_logica, sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(buffer_params->stream + desplazamiento, &tamanio, sizeof(uint32_t));

    // 3. Creamos la estructura de la syscall
    return crear_t_syscall(tipo, buffer_params);
}

t_syscall* preparar_syscall_init_proc(char* path_instrucciones, int prioridad) {
    t_buffer* buffer_params = malloc(sizeof(t_buffer));
    
    // Calculamos el tamaño del string (con \0)
    int length_path = strlen(path_instrucciones) + 1;
    
    // El tamaño total será: tamaño del string (int) + el string + la prioridad (int)
    buffer_params->size = sizeof(int) + length_path + sizeof(int);
    buffer_params->stream = malloc(buffer_params->size);

    int desplazamiento = 0;

    // 1. Metemos el tamaño del path (para que el Kernel sepa cuánto leer)
    memcpy(buffer_params->stream + desplazamiento, &length_path, sizeof(int));
    desplazamiento += sizeof(int);

    // 2. Metemos el path real
    memcpy(buffer_params->stream + desplazamiento, path_instrucciones, length_path);
    desplazamiento += length_path;

    // 3. Metemos la prioridad
    memcpy(buffer_params->stream + desplazamiento, &prioridad, sizeof(int));

    return crear_t_syscall(SC_INIT_PROC, buffer_params);
}

t_syscall* preparar_syscall_exit() {
    t_buffer* buffer_vacio = malloc(sizeof(t_buffer));
    buffer_vacio->size = 0;
    buffer_vacio->stream = NULL; 

    return crear_t_syscall(SC_EXIT, buffer_vacio);
}

t_sc_sleep* deserializar_sc_sleep(t_syscall* syscall) {
    t_sc_sleep* datos = malloc(sizeof(t_sc_sleep));
    memcpy(&(datos->tiempo), syscall->buffer->stream, sizeof(int));
    return datos;
}

t_sc_mem_free* deserializar_sc_mem_free(t_syscall* syscall) {
    t_sc_mem_free* datos = malloc(sizeof(t_sc_mem_free));
    memcpy(&(datos->id_segmento), syscall->buffer->stream, sizeof(int));
    return datos;
}

t_sc_mutex* deserializar_sc_mutex(t_syscall* syscall) {
    t_sc_mutex* datos = malloc(sizeof(t_sc_mutex));
    // El stream ya contiene el '\0', así que duplicamos la cadena
    datos->nombre_mutex = strdup((char*)syscall->buffer->stream);
    return datos;
}

t_sc_mem_alloc* deserializar_sc_mem_alloc(t_syscall* syscall) {
    t_sc_mem_alloc* datos = malloc(sizeof(t_sc_mem_alloc));
    int offset = 0;
    memcpy(&(datos->id_segmento), syscall->buffer->stream + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&(datos->tamanio), syscall->buffer->stream + offset, sizeof(int));
    return datos;
}

t_sc_io* deserializar_sc_io(t_syscall* syscall) {
    t_sc_io* datos = malloc(sizeof(t_sc_io));
    int offset = 0;
    memcpy(&(datos->direccion), syscall->buffer->stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&(datos->tamanio), syscall->buffer->stream + offset, sizeof(uint32_t));
    return datos;
}

t_sc_init* deserializar_sc_init(t_syscall* syscall) {
    t_sc_init* datos = malloc(sizeof(t_sc_init));
    int offset = 0;
    int len_path;

    // 1. Leer tamaño del path
    memcpy(&len_path, syscall->buffer->stream + offset, sizeof(int));
    offset += sizeof(int);

    // 2. Leer el path (usamos malloc + memcpy para asegurar el tamaño exacto)
    datos->path = malloc(len_path);
    memcpy(datos->path, syscall->buffer->stream + offset, len_path);
    offset += len_path;

    // 3. Leer prioridad
    memcpy(&(datos->prioridad), syscall->buffer->stream + offset, sizeof(int));

    return datos;
}

void enviar_syscall(t_syscall* syscall, int socket_kernel) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = SYSCALL;
    paquete->buffer = malloc(sizeof(t_buffer));

    // El tamaño será: el tipo de syscall (int) + el tamaño de los parámetros (buffer->size)
    paquete->buffer->size = sizeof(int) + syscall->buffer->size;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int desplazamiento = 0;
    // 1. Metemos el tipo de syscall (ej: SC_SLEEP)
    memcpy(paquete->buffer->stream + desplazamiento, &(syscall->tipo_syscall), sizeof(int));
    desplazamiento += sizeof(int);

    // 2. Metemos los parámetros (el contenido del buffer del t_syscall)
    memcpy(paquete->buffer->stream + desplazamiento, syscall->buffer->stream, syscall->buffer->size);

    int bytes = paquete->buffer->size + 2 * sizeof(int);
    // Usamos la versión de serializar que devuelve el tamaño calculado
    void* a_enviar = serializar_paquete(paquete, bytes); //MODIFIQUE, BORRE , YA QUE ESTABA LLAMANDO MASL ANTES , serializar_paquete, pasándole &bytes cuando la función espera el tamaño como entero

    send(socket_kernel, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

t_syscall* recibir_t_syscall(int socket_cliente) {
    int size;
    // Usamos tu función recibir_buffer que ya sabe leer el tamaño y el stream
    void* buffer_recibido = recibir_buffer(&size, socket_cliente);
    
    t_syscall* syscall = malloc(sizeof(t_syscall));
    int desplazamiento = 0;

    // 1. Extraemos el tipo de syscall
    memcpy(&(syscall->tipo_syscall), buffer_recibido + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);

    // 2. Extraemos el buffer de parámetros
    syscall->buffer = malloc(sizeof(t_buffer));
    syscall->buffer->size = size - sizeof(int); // El resto del stream son parámetros
    syscall->buffer->stream = malloc(syscall->buffer->size);
    memcpy(syscall->buffer->stream, buffer_recibido + desplazamiento, syscall->buffer->size);

    free(buffer_recibido);
    return syscall;
}

void eliminar_t_syscall(t_syscall* syscall) {
    if (syscall != NULL) {
        if (syscall->buffer != NULL) {
            free(syscall->buffer->stream);
            free(syscall->buffer);
        }
        free(syscall);
    }
}