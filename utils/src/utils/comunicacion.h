#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <assert.h>
#include <pthread.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

extern t_log* logger;

#define CONEXION_RETRY_INTERVAL 3

// =========================
// CÓDIGOS DE OPERACIÓN (ÚNICO ENUM)
// =========================
typedef enum
{
    // GENERALES
    MENSAJE,
    PAQUETE,

    // IDENTIFICACION DE MODULOS
    MENSAJE_CPU,
    MENSAJE_SCHEDULER,
    MENSAJE_MEMORY_STICK,
    MENSAJE_SWAP,
    MENSAJE_IO,

    // =========================
    // KERNEL SCHEDULER -> MEMORY
    // =========================
    CREATE_PROCESS,
    FINISH_PROCESS,
    SUSPEND_PROCESS,
    DESUSPEND_PROCESS,

    CREATE_SEGMENT,
    DELETE_SEGMENT,

    MEM_READ,
    MEM_WRITE,

    REQUEST_COMPACTATION,

	// memory sticks/swap <-> cpu/memory
	ESCRITURA,
	LECTURA,
    // =========================
    // RESPUESTAS MEMORY -> SCHEDULER
    // =========================
    MEM_OK,
    MEM_FULL,
    MEM_COMPACTACION,
    MEM_ERROR,
    MEM_AMPLIADA,
    MEM_CORRUPTA,

	// =========================    
	// RESPUESTAS SCHEDULER/MEMORY → IO    
	// =========================
	IO_SLEEP,
	IO_STDIN, 
	IO_STDOUT, 

	// memory <-> CPU
	INSTRUCCION,
	INSTRUCCION_ERROR,
	CONTEXTO_EJEC,
	SOLICITUD_CONTEXTO, 
	SEGMENT_MAX_SIZE,
	TABLA_SEGMENTOS,
	LIST_MEMORY_STICKS,
	MEMORY_STICKS, 

	// swap -> memory
	SWAP_BLOCK, 
    SWAP_READ_BLOCK,
    SWAP_WRITE_BLOCK,

	// scheduler <-> CPU
	SYSCALL,
    INTERRUPCION_CPU,
	CPU_DESALOJADA,
	CPU_ERROR,
	CPU_SEG_FAULT,
	
    // =========================
    // SCHEDULER -> CPU
    // =========================
    EJECUTAR_PROCESO,     // lleva solo PID


    // ERROR GLOBAL
    BSOD,

	DESCONEXION = -1

} op_code;

// =========================
// ESTRUCTURAS BASE
// =========================

typedef struct
{
    int size;
    void* stream;
} t_buffer;

typedef struct
{
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;

typedef struct {
    char ip[16];       // IPv4 máx "255.255.255.255" + '\0'
    char puerto[6];    // Puerto máx "65535" + '\0'
    uint32_t tamanio;
} t_memory_stick_info;



void enviar_codigo(op_code codigo, int socket);


/**
* @brief Compacta un paquete en un flujo de bytes contiguos para ser enviado por red.
* @param paquete Puntero a la estructura del paquete a serializar.
* @param bytes Referencia al entero donde se almacenará el tamaño total del stream resultante.
* @return Puntero al flujo de bytes (void*) serializado.
*/
void* serializar_paquete(t_paquete* paquete, int bytes);

/**
 * @brief Establece una conexión con un servidor remoto.
 *        Reintenta automaticamente cada CONEXION_RETRY_INTERVAL segundos
 *        hasta que el servidor esta disponible (reintento infinito).
 * @param ip Dirección IP del servidor.
 * @param puerto Puerto de escucha del servidor.
 * @return File descriptor del socket de la conexion.
 *         Solo retorna -1 si la IP no puede resolverse.
 */
int crear_conexion(char *ip, char* puerto);

/**
* @brief Envía una cadena de texto simple a través de un socket.
* @param mensaje Puntero al string a enviar.
* @param socket_cliente Socket activo de la conexión.
*/
void enviar_mensaje(char* mensaje, int socket_cliente);

/**
* @brief Inicializa el buffer interno de un paquete preexistente.
* @param paquete Puntero al paquete al que se le creará el buffer.
*/
void crear_buffer(t_paquete* paquete);

/**
* @brief Crea y reserva memoria para una nueva estructura de paquete.
* @return Puntero al paquete (t_paquete*) inicializado.
*/
t_paquete* crear_paquete(void);

/**
* @brief Agrega un flujo de datos al buffer de un paquete dinámicamente.
* @param paquete Paquete donde se insertarán los datos.
* @param valor Puntero a los datos que se desean agregar.
* @param tamanio Cantidad de bytes a copiar en el buffer.
*/
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

/**
* @brief Serializa y transmite un paquete completo a través del socket.
* @param paquete Paquete a enviar.
* @param socket_cliente Socket de la conexión de destino.
*/
void enviar_paquete(t_paquete* paquete, int socket_cliente);

/**
* @brief Libera la memoria ocupada por un paquete y su buffer interno.
* @param paquete Puntero al paquete a eliminar.
*/
void eliminar_paquete(t_paquete* paquete);

/**
* @brief Cierra el socket de conexión y libera recursos de red.
* @param socket_cliente File descriptor del socket a cerrar.
*/
void liberar_conexion(int socket_cliente);

// --- SERVIDOR ---

/**
* @brief Configura un socket de escucha en un puerto específico.
* @param puerto Puerto donde el servidor aceptará conexiones.
* @return File descriptor del socket del servidor (listen), o -1 en caso de error.
*/
int iniciar_servidor(char* puerto);

/**
* @brief Bloquea la ejecución hasta que un cliente intenta conectarse.
* @param socket_servidor Socket de escucha del servidor.
* @return File descriptor del socket del cliente aceptado.
*/
int esperar_cliente(int socket_servidor);

/**
* @brief Recibe el primer byte/entero de un mensaje para identificar el tipo de operación.
* @param socket_cliente Socket del cliente conectado.
* @return Código de operación (op_code) recibido, o -1 si hubo desconexión.
*/
int recibir_operacion(int socket_cliente);

/**
* @brief Recibe el stream de datos crudos provenientes de un socket.
* @param size Puntero donde se almacenará el tamaño del buffer recibido.
* @param socket_cliente Socket del cliente.
* @return Puntero al buffer con los datos recibidos.
*/
void* recibir_buffer(int* size, int socket_cliente);

/**
* @brief Recibe y muestra por consola un mensaje de texto plano.
* @param socket_cliente Socket del cliente.
*/
void recibir_mensaje(int socket_cliente);

/**
* @brief Deserializa un paquete complejo recibido en una lista de elementos.
* @param socket_cliente Socket del cliente.
* @return Puntero a una t_list con los contenidos del paquete.
*/
t_list* recibir_paquete(int socket_cliente);

/**
* @brief Inicia un servidor capaz de manejar múltiples clientes mediante hilos o select/poll.
* @param puerto Puerto de escucha.
* @param server_fd Puntero donde se guardará el FD del servidor creado.
* @param atender_cliente Puntero a la función que gestionará la lógica de cada cliente.
*/
void iniciar_servidor_multiplexador(char *puerto, int *server_fd, void* (*atender_cliente)(void*));

/**
* @brief Obtiene ip del cliente
* @param cliente_fd File descriptor del cliente
* @return Puntero a un string con ip
*/
char* obtener_ip_cliente(int cliente_fd);
#endif
