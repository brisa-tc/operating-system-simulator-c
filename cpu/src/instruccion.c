#include "globals.h"
#include "instruccion.h"
#include <utils/interrupcion.h>
#include <utils/syscall.h>
#include <stdbool.h>

/*
NOOP: Representa la instrucción No Operation, es decir, va a consumir solamente el tiempo del ciclo de instrucción.
SET (Registro, Valor): Asigna al registro el valor pasado como parámetro.
MOV_IN (Registro Datos): Lee el valor de memoria correspondiente a la Dirección Lógica que se encuentra en el Registro SI y lo almacena en el Registro Datos.
MOV_OUT (Registro Datos): Lee el valor del Registro Datos y lo escribe en la dirección física de memoria obtenida a partir de la Dirección Lógica almacenada en el Registro DI.
SUM (Registro Destino, Registro Origen): Suma al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
SUB (Registro Destino, Registro Origen): Resta al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
JNZ (Registro, Instrucción): Si el valor del registro es distinto de cero, actualiza el program counter al número de instrucción pasada por parámetro.
COPY_MEM (Registro Tamaño): Toma la dirección apuntada por el registro SI y copia la cantidad de bytes indicadas en el registro tamaño a la posición de memoria apuntada por el registro DI.
Las siguientes instrucciones se considerarán Syscalls, ya que las mismas no pueden ser resueltas por la CPU y depende de la acción del Kernel Scheduler para su realización. A diferencia de la vida real donde la llamada es a una única instrucción, para simplificar la comprensión de los scripts, vamos a utilizar un nombre diferente para cada Syscall. La descripción de lo que hace cada una va a estar detallada en el Kernel Scheduler.
MUTEX_CREATE (Nombre Mutex)
MUTEX_LOCK (Nombre Mutex)
MUTEX_UNLOCK (Nombre Mutex)
MEM_ALLOC (Id del Segmento, Tamaño)
MEM_FREE (Id del Segmento)
SLEEP (Tiempo)
STDOUT (Registro Dirección Lógica, Registro Tamaño)
STDIN (Registro Dirección Lógica, Registro Tamaño)
INIT_PROC (Archivo de instrucciones, Prioridad)
EXIT
*/

static void enviar_syscall_a_kernel(t_syscall* syscall) {
    if (socket_kernel_scheduler == -1) {
        eliminar_t_syscall(syscall);
        return;
    }

    enviar_syscall(syscall, socket_kernel_scheduler);
    eliminar_t_syscall(syscall);

    pthread_mutex_lock(&mutex_estado_cpu);
    cpu_tiene_proceso = false;
    pid_actual = -1;
    pthread_mutex_unlock(&mutex_estado_cpu);
}

static void finalizar_por_resultado_memoria(t_resultado_memoria_cpu resultado) {
    if (resultado == MEM_CPU_OK)
        return;

    pthread_mutex_lock(&mutex_estado_cpu);
    int pid_con_error = pid_actual;
    pthread_mutex_unlock(&mutex_estado_cpu);

    enviar_contexto_ejecucion(registros, socket_kernel_memory);

    if (resultado == MEM_CPU_SEG_FAULT)
        enviar_seg_fault_cpu(pid_con_error, socket_kernel_scheduler);
    else
        enviar_error_cpu(pid_con_error, socket_kernel_scheduler);

    pthread_mutex_lock(&mutex_estado_cpu);
    cpu_tiene_proceso = false;
    pid_actual = -1;
    pthread_mutex_unlock(&mutex_estado_cpu);
}

void ejecutar_NOOP(char** params) {
    // Simplemente consume un ciclo de instrucción
    log_debug(logger, "[CPU] Ejecutando NOOP: Esperando 1 ciclo...");
}

void ejecutar_SET(char** params) {
    // params[0]: Registro, params[1]: Valor
    char* registro = params[0];
    uint32_t valor = (uint32_t)atoi(params[1]);

    set_registro(registro, valor);
    log_debug(logger, "[CPU] SET: Asignando %d al registro %s", valor, registro);
}

void ejecutar_MOV_IN(char** params) {
    // params[0]: Registro Datos
    // Lee de memoria usando la dirección en el Registro SI
    char* registro = params[0];
    t_resultado_memoria_cpu resultado = move_in_registros(registro);
    finalizar_por_resultado_memoria(resultado);
    log_debug(logger, "[CPU] MOV_IN: Leyendo de memoria (dir: Registro SI) hacia %s", params[0]);
}

void ejecutar_MOV_OUT(char** params) {
    // params[0]: Registro Datos
    // Escribe en memoria usando la dirección en el Registro DI
    char* registro = params[0];
    t_resultado_memoria_cpu resultado = move_out_registros(registro);
    finalizar_por_resultado_memoria(resultado);
    log_debug(logger, "[CPU] MOV_OUT: Escribiendo valor de %s en memoria (dir: Registro DI)", params[0]);
}

void ejecutar_SUM(char** params) {
    // params[0]: Destino, params[1]: Origen
    char* registro_dest = params[0];
    char* registro_orig = params[1];

    sumar_registros(registro_dest, registro_orig);
    log_debug(logger, "[CPU] SUM: %s = %s + %s", params[0], params[0], params[1]);
}

void ejecutar_SUB(char** params) {
    // params[0]: Destino, params[1]: Origen
    char* registro_dest = params[0];
    char* registro_orig = params[1];

    restar_registros(registro_dest, registro_orig);
    log_debug(logger, "[CPU] SUB: %s = %s - %s", params[0], params[0], params[1]);
}

void ejecutar_JNZ(char** params) {
    // params[0]: Registro, params[1]: Instrucción (PC)
    char* registro = params[0];
    u_int32_t valor = (u_int32_t)atoi(params[1]);

    jnz_registros(registro, valor);
    log_debug(logger, "[CPU] JNZ: Si %s != 0, saltar a instrucción %s", params[0], params[1]);
}

void ejecutar_COPY_MEM(char** params) {
    // params[0]: Registro Tamaño
    t_resultado_memoria_cpu resultado = copy_mem_registros(params[0]);
    finalizar_por_resultado_memoria(resultado);
    log_debug(logger, "[CPU] COPY_MEM: Copiando %s bytes desde SI hacia DI", params[0]);
}

void ejecutar_MUTEX_CREATE(char** params) {
    log_debug(logger, "[SYSCALL] MUTEX_CREATE: Solicitando creación de mutex '%s'", params[0]);
    enviar_syscall_a_kernel(preparar_syscall_mutex(SC_MUTEX_CREATE, params[0]));
}

void ejecutar_MUTEX_LOCK(char** params) {
    log_debug(logger, "[SYSCALL] MUTEX_LOCK: Intentando bloquear mutex '%s'", params[0]);
    enviar_syscall_a_kernel(preparar_syscall_mutex(SC_MUTEX_LOCK, params[0]));
}

void ejecutar_MUTEX_UNLOCK(char** params) {
    log_debug(logger, "[SYSCALL] MUTEX_UNLOCK: Liberando mutex '%s'", params[0]);
    enviar_syscall_a_kernel(preparar_syscall_mutex(SC_MUTEX_UNLOCK, params[0]));
}

void ejecutar_MEM_ALLOC(char** params) {
    // params[0]: Id Segmento, params[1]: Tamaño
    log_debug(logger, "[SYSCALL] MEM_ALLOC: Solicitando %s bytes para segmento %s", params[1], params[0]);
    enviar_syscall_a_kernel(preparar_syscall_mem_alloc(atoi(params[0]), atoi(params[1])));
}

void ejecutar_MEM_FREE(char** params) {
    log_debug(logger, "[SYSCALL] MEM_FREE: Liberando segmento %s", params[0]);
    enviar_syscall_a_kernel(preparar_syscall_mem_free(atoi(params[0])));
}

void ejecutar_SLEEP(char** params) {
    log_debug(logger, "[SYSCALL] SLEEP: Bloqueando proceso por %s milisegundos", params[0]);
    enviar_syscall_a_kernel(preparar_syscall_sleep(atoi(params[0])));
    //desalojar_proceso_actual();
}

void ejecutar_STDIN(char** params) {
    // params[0]: Reg. Dirección Lógica, params[1]: Reg. Tamaño
    log_debug(logger, "[SYSCALL] STDIN: Leyendo entrada hacia dirección en %s (%s bytes)", params[0], params[1]);
    enviar_syscall_a_kernel(preparar_syscall_io(SC_STDIN, get_registro(params[0]), get_registro(params[1])));
    //desalojar_proceso_actual();
}

void ejecutar_STDOUT(char** params) {
    log_debug(logger, "[SYSCALL] STDOUT: Escribiendo desde dirección en %s (%s bytes)", params[0], params[1]);
    enviar_syscall_a_kernel(preparar_syscall_io(SC_STDOUT, get_registro(params[0]), get_registro(params[1])));
    //desalojar_proceso_actual();
}

void ejecutar_INIT_PROC(char** params) {
    // params[0]: Archivo, params[1]: Prioridad
    log_debug(logger, "[SYSCALL] INIT_PROC: Creando proceso desde %s con prioridad %s", params[0], params[1]);
    enviar_syscall_a_kernel(preparar_syscall_init_proc(params[0], atoi(params[1])));
}

void ejecutar_EXIT(char** params) {
    log_debug(logger, "[SYSCALL] EXIT: Finalizando el proceso actual.");
    enviar_syscall_a_kernel(preparar_syscall_exit());
    //desalojar_proceso_actual();
}

// Para las que no existen aún, puedes usar una función genérica
void instruccion_no_implementada(char** params) {
    log_error(logger, "Instrucción no implementada o no reconocida.");
}
