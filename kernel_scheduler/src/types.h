#ifndef TYPES_H
#define TYPES_H
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <utils/comunicacion.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

//LOOOGS 
typedef struct {
    int conexion_memoria;
    t_log* logger;
    t_config* config;
} t_kernel_scheduler;


// =========================
// ESTADOS DEL PROCESO

typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCK,
    SUSP_BLOCK,
    SUSP_READY,
    EXIT
} t_estado;


// =========================
// PCB (PROCESS CONTROL BLOCK)

typedef struct {
    int pid;
    int prioridad;
    int prioridad_original;
    t_estado estado;

    //NECESARIO para mediano plazo
    time_t tiempo_suspendido;

    int intentos_memoria;

} t_pcb;

//MEDIANO PLAZO (BLOCK TRACKING)
typedef struct {
    t_pcb* pcb;
    time_t inicio_block;
} t_block_entry;

//t_block_entry sirve para:
//medir tiempo en BLOCK
//decidir cuándo suspender un proceso
//implementar el mediano plazo correctamente
//no ensuciar el PCB


// =========================
// MUTEX KERNEL
// =========================
typedef struct {

    char* nombre;
    t_pcb* duenio;
    t_list* bloqueados;

} t_mutex_kernel;


//ENUM PLANIFICACIÓN
typedef enum {
    ALGORITMO_FIFO,
    ALGORITMO_RR,
    ALGORITMO_CMN
} t_algoritmo_planificacion;


//Colas
typedef enum {
    COLA_FIFO,
    COLA_RR
} t_algoritmo_cola;

//DESALOJO
typedef enum {
    MOTIVO_FIN_QUANTUM,
    MOTIVO_PRIORIDAD,
    MOTIVO_COMPACTACION,
    MOTIVO_SEG_FAULT
} t_motivo_desalojo;

//CPU STRUCT
typedef struct {
    int id;
    int socket_dispatch;
    int socket_interrupt;
    bool libre;
    t_pcb* pcb;

    bool tiene_interrupcion_pendiente;
    t_motivo_desalojo motivo_interrupcion;
    uint64_t generacion_despacho;
    time_t desconectado_en;
} t_cpu_kernel;

typedef struct {
    op_code tipo;
    int socket;
    bool ocupado;
    t_pcb* pcb;
    uint32_t direccion;
    uint32_t tamanio;
    int tiempo;
} t_io_kernel;

typedef struct {
    op_code tipo;
    t_pcb* pcb;
    uint32_t direccion;
    uint32_t tamanio;
    int tiempo;
} t_pedido_io;

//quantums
typedef struct {
    t_pcb* pcb;
    t_cpu_kernel* cpu;
    uint64_t generacion_despacho;
} t_quantum_args;





#endif
