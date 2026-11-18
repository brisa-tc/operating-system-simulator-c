#ifndef SYSCALL_H
#define SYSCALL_H

#include "comunicacion.h"

typedef enum {
    SC_MUTEX_CREATE,
    SC_MUTEX_LOCK,
    SC_MUTEX_UNLOCK,
    SC_MEM_ALLOC,
    SC_MEM_FREE,
    SC_SLEEP,
    SC_STDOUT,
    SC_STDIN,
    SC_INIT_PROC,
    SC_EXIT
} e_tipo_syscall;

typedef struct {
    e_tipo_syscall tipo_syscall;
    t_buffer* buffer;     // Parámetros empaquetados
} t_syscall;

typedef struct { int tiempo; } t_sc_sleep;
typedef struct { char* nombre_mutex; } t_sc_mutex;
typedef struct { int id_segmento; int tamanio; } t_sc_mem_alloc;
typedef struct { int id_segmento; } t_sc_mem_free;
typedef struct { uint32_t direccion; uint32_t tamanio; } t_sc_io;
typedef struct { char* path; int prioridad; } t_sc_init;

/**
 * @brief Crea una estructura t_syscall lista para ser enviada o procesada.
 * @param tipo El tipo de syscall (e_tipo_syscall).
 * @param buffer Puntero al buffer que ya contiene los parámetros serializados.
 * @return Puntero a la t_syscall creada en el heap.
 */
t_syscall* crear_t_syscall(e_tipo_syscall tipo, t_buffer* buffer);

/**
 * @brief Serializa y envía una estructura t_syscall a través de un socket.
 * @param syscall Puntero a la estructura syscall a enviar.
 * @param socket_kernel Descriptor del socket del proceso Kernel.
 */
void enviar_syscall(t_syscall* syscall, int socket_kernel);

/**
 * @brief Recibe un flujo de bytes de un socket y lo reconstruye en una t_syscall.
 * @param socket_cliente Descriptor del socket de donde provienen los datos.
 * @return Puntero a la estructura t_syscall recibida.
 */
t_syscall* recibir_t_syscall(int socket_cliente); //scheduler

/**
 * @brief Libera la memoria de una t_syscall, incluyendo su buffer y el stream interno.
 * @param syscall Puntero a la estructura a eliminar.
 */
void eliminar_t_syscall(t_syscall* syscall);

/**
 * @brief Prepara una syscall relacionada con Mutex (Create, Lock, Unlock).
 * @param tipo El tipo de syscall (SC_MUTEX_CREATE, SC_MUTEX_LOCK, o SC_MUTEX_UNLOCK).
 * @param nombre_mutex El nombre del mutex como cadena de caracteres.
 * @return Puntero a la t_syscall lista para ser enviada.
 */
t_syscall* preparar_syscall_mutex(e_tipo_syscall tipo, char* nombre_mutex);

/**
 * @brief Prepara una syscall de MEM_ALLOC con ID y Tamaño.
 * @param id_segmento Identificador numérico del segmento.
 * @param tamanio Tamaño en bytes a asignar.
 * @return Puntero a la t_syscall inicializada con los dos enteros en su buffer.
 */
t_syscall* preparar_syscall_mem_alloc(int id_segmento, int tamanio);

/**
 * @brief Prepara una syscall de MEM_FREE para liberar un segmento.
 * @param id_segmento Identificador del segmento a liberar.
 * @return Puntero a la t_syscall inicializada.
 */
t_syscall* preparar_syscall_mem_free(int id_segmento);

/**
 * @brief Prepara una syscall de SLEEP.
 * @param tiempo_ms Tiempo en milisegundos que el proceso debe bloquearse.
 * @return Puntero a la t_syscall inicializada.
 */
t_syscall* preparar_syscall_sleep(int tiempo_ms);

/**
 * @brief Prepara una syscall de STDIN o STDOUT recibiendo los valores ya procesados.
 * @param tipo SC_STDIN o SC_STDOUT.
 * @param direccion_logica Valor numérico de la dirección de memoria.
 * @param tamanio Valor numérico del tamaño de la operación.
 * @return Puntero a la t_syscall inicializada con los parámetros en su buffer.
 */
t_syscall* preparar_syscall_io(e_tipo_syscall tipo, uint32_t direccion_logica, uint32_t tamanio);

/**
 * @brief Prepara una syscall de INIT_PROC.
 * @param path_instrucciones Ruta o nombre del archivo de instrucciones.
 * @param prioridad Valor numérico de la prioridad del proceso.
 * @return Puntero a la t_syscall inicializada.
 */
t_syscall* preparar_syscall_init_proc(char* path_instrucciones, int prioridad);

/**
 * @brief Prepara una syscall de EXIT sin parámetros.
 * @return Puntero a la t_syscall inicializada con buffer vacío.
 */
t_syscall* preparar_syscall_exit();

/**
 * @brief Extrae los datos de una syscall de SLEEP hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_sleep con el tiempo extraído.
 */
t_sc_sleep* deserializar_sc_sleep(t_syscall* syscall);

/**
 * @brief Extrae los datos de una syscall de MEM_FREE hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_mem_free con el ID de segmento extraído.
 */
t_sc_mem_free* deserializar_sc_mem_free(t_syscall* syscall);

/**
 * @brief Extrae los datos de una syscall de Mutex hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_mutex con el nombre del mutex duplicado.
 */
t_sc_mutex* deserializar_sc_mutex(t_syscall* syscall);

/**
 * @brief Extrae los datos de una syscall de MEM_ALLOC hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_mem_alloc con el ID y tamaño extraídos.
 */
t_sc_mem_alloc* deserializar_sc_mem_alloc(t_syscall* syscall);

/**
 * @brief Extrae los datos de una syscall de I/O (STDIN/STDOUT) hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_io con la dirección y tamaño extraídos.
 */
t_sc_io* deserializar_sc_io(t_syscall* syscall);

/**
 * @brief Extrae los datos de una syscall de INIT_PROC hacia un struct específico.
 * @param syscall Estructura genérica recibida por socket.
 * @return Puntero a t_sc_init con el path y prioridad extraídos.
 */
t_sc_init* deserializar_sc_init(t_syscall* syscall);

/*
void procesar_syscall(t_syscall* syscall_generica) {
    switch(syscall_generica->tipo_syscall) {
        case SC_SLEEP:
            t_sc_sleep* datos = deserializar_sc_sleep(syscall_generica);
            log_info(logger, "Ejecutando Sleep por %d ms", datos->tiempo);
            // logic_kernel_sleep(datos->tiempo);
            free(datos); 
            break;

        case SC_INIT_PROC:
            t_sc_init* datos_init = deserializar_sc_init(syscall_generica);
            log_info(logger, "Creando proceso: %s", datos_init->path);
            // logic_kernel_init(datos_init->path, datos_init->prioridad);
            free(datos_init->path);
            free(datos_init);
            break;
            
        // ... otros casos ...
    }
}
*/

#endif