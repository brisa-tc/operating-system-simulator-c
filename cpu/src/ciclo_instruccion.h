#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include "instruccion.h"

typedef struct {
    int pid;
    bool es_desalojo;
} t_registro_interrupcion;

extern t_list* lista_interrupciones;
extern pthread_mutex_t mutex_lista_interrupciones;

extern pthread_mutex_t mutex_ciclo;
extern pthread_cond_t cond_ciclo;
extern bool ciclo_activo;

extern pthread_mutex_t mutex_instruccion;
extern pthread_cond_t cond_instruccion;
extern char* buffer_instruccion;
extern int op_code_instruccion;
extern bool respuesta_instruccion_lista;

typedef enum
{
    NOOP,
    SET,
    MOV_IN,
    MOV_OUT,
    SUM,
    SUB,
    JNZ,
    COPY_MEM,
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    MEM_ALLOC,
    MEM_FREE,
    SLEEP,
    STDIN,
    STDOUT,
    INIT_PROC,
    EXIT,
    INSTRUCCION_INVALIDA
} e_instruccion;

typedef struct{
    e_instruccion tipo_instruccion;
    char** parametros;
}t_instruccion;


/**
 * @brief Ejecuta el ciclo de instrucción completo: Fetch, Decode, Execute.
 */
void ciclo_instruccion(void);
void check_interrupt(void);
void* atender_ciclo(void* arg);

/**
 * @brief Obtiene la siguiente línea de instrucción desde la Memoria.
 * @return String con la instrucción cruda. Debe ser liberado.
 */
char* fetch(void);

/**
 * @brief Convierte un string de instrucción en una estructura t_instruccion.
 * @param linea String obtenido en el fetch.
 * @return Puntero a estructura t_instruccion inicializada.
 */
t_instruccion* decode(char* linea);

/**
 * @brief Ejecuta la lógica asociada a la instrucción a través de la dispatch_table.
 * @param instruccion Puntero a la instrucción a ejecutar.
 */
void execute(t_instruccion* instruccion);

/**
 * @brief Convierte el nombre de una instrucción en su valor de enumeración.
 * @param comando Nombre del comando (ej: "SET").
 * @return Valor del enum e_instruccion correspondiente.
 */
e_instruccion string_a_enum(char* comando);

/**
 * @brief Convierte un tipo de instrucción en su nombre legible para logs.
 * @param tipo Valor del enum e_instruccion.
 * @return String con el nombre de la instrucción (ej: "SET").
 */
char* nombre_instruccion(e_instruccion tipo);

/**
 * @brief Convierte un array de parámetros en un string legible para logs.
 * @param parametros Array de strings con los parámetros (terminado en NULL).
 * @return String con los parámetros concatenados (ej: "BX 25"). Debe ser liberado.
 */
char* parametros_a_string(char** parametros);
/**
 * @brief Libera la memoria de una instrucción y sus parámetros.
 * @param ins Puntero a la instrucción a liberar.
 */
void liberar_instruccion(t_instruccion* ins);
void liberar_cpu(bool notificar_desalojo);

// Recibe el array de strings con los parámetros
typedef void (*t_instruccion_func)(char**);

#endif
