#ifndef ESTRUCTURA_H_
#define ESTRUCTURA_H_

#include <commons/config.h>
#include <commons/log.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include "utils/memorySticks.h"
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// ─── Listas globales ──────────────────────────────────────────────────────────

extern t_list* lista_contextos;
extern t_list* lista_segmentos;       // lista global de todos los t_segmento*
extern t_list* lista_huecos;
extern t_list* lista_memory_sticks;   // lista de t_memoryStick_info*
extern t_list* lista_cpus;            // lista de t_cpu_conectada*

// ─── Sockets de módulos únicos ────────────────────────────────────────────────

extern int socket_kernel_scheduler;
extern int socket_swap;
extern int swap_block_size;
extern int swap_file_size;
extern int swap_block_count;
extern char* bitmap_swap_storage;
extern t_bitarray* bitmap_swap;

// ─── Mutexes globales ─────────────────────────────────────────────────────────

extern pthread_mutex_t mutex_contextos;
extern pthread_mutex_t mutex_segmentos;         // protege tabla por proceso
extern pthread_mutex_t mutex_segmentos_global;  // protege lista_segmentos global
extern pthread_mutex_t mutex_huecos;
extern pthread_mutex_t mutex_memory_sticks;
extern pthread_mutex_t mutex_cpus;
extern pthread_mutex_t mutex_swap;
extern pthread_mutex_t mutex_segmentos_en_swap;

// ─── Variables de config ──────────────────────────────────────────────────────

extern int segment_max_size;
extern char* allocation_strategy;
extern int instruction_delay;
extern int compaction_delay;
extern char* scripts_basepath;
extern char* puerto_escucha;
extern char* log_level;

extern t_config* config;
extern t_log* logger;

// ─── Tipos ────────────────────────────────────────────────────────────────────

typedef struct {
    uint32_t PC;
    uint8_t  AX;
    uint8_t  BX;
    uint8_t  CX;
    uint8_t  DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;
} t_registros;

typedef struct {
    int      id;
    uint32_t base;
    uint32_t tamanio;
} t_segmento;

typedef struct {
    int      pid;
    t_list*  segmentos;
} t_tabla_segmentos;

typedef struct {
    int id_segmento;
    uint32_t tamanio_segmento;
    t_list* bloques;
} t_segmento_swap;

typedef struct {
    int              pid;
    t_registros      registros;
    t_tabla_segmentos* tabla_segmentos;
    t_list*          segmentos_en_swap;
    char*            path_instrucciones;
} t_contexto;

typedef struct {
    uint32_t base;
    uint32_t tamanio;
} t_hueco;

typedef struct {
    int id;
    int socket;
    int pid_actual;
} t_cpu_conectada;



// ─── Funciones de inicialización ──────────────────────────────────────────────

t_config* iniciar_config(char* path_config);
void      cargar_config(t_config* config);
void      iniciar_logger();
void      iniciar_estructuras();
void      inicializar_bitmap_swap(int block_size, int file_size);
int       obtener_bloque_swap_libre(void);
bool      hay_bloques_swap_disponibles(int cantidad);
void      marcar_bloque_swap_ocupado(int bloque);
void      marcar_bloque_swap_libre(int bloque);
int       reservar_bloque_swap(void);
void      liberar_bloques_swap(t_list* bloques);

#endif
