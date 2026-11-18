#include "globals.h"
#include "ciclo_instruccion.h"
#include <utils/interrupcion.h>
#include <utils/instruccion.h>

t_instruccion_func dispatch_table[] = {
    [NOOP] = ejecutar_NOOP,
    [SET] = ejecutar_SET,
    [MOV_IN] = ejecutar_MOV_IN,
    [MOV_OUT] = ejecutar_MOV_OUT,
    [SUM] = ejecutar_SUM,
    [SUB] = ejecutar_SUB,
    [JNZ] = ejecutar_JNZ,
    [COPY_MEM] = ejecutar_COPY_MEM,
    [MUTEX_CREATE] = ejecutar_MUTEX_CREATE,
    [MUTEX_LOCK] = ejecutar_MUTEX_LOCK,
    [MUTEX_UNLOCK] = ejecutar_MUTEX_UNLOCK,
    [MEM_ALLOC] = ejecutar_MEM_ALLOC,
    [MEM_FREE] = ejecutar_MEM_FREE,
    [SLEEP] = ejecutar_SLEEP,
    [STDIN] = ejecutar_STDIN,
    [STDOUT] = ejecutar_STDOUT,
    [INIT_PROC] = ejecutar_INIT_PROC,
    [EXIT] = ejecutar_EXIT,
    [INSTRUCCION_INVALIDA] = instruccion_no_implementada
};

static bool instruccion_es_syscall(e_instruccion tipo) {
    return tipo >= MUTEX_CREATE && tipo <= EXIT;
}

t_list* lista_interrupciones = NULL;
pthread_mutex_t mutex_lista_interrupciones = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_ciclo = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_ciclo = PTHREAD_COND_INITIALIZER;
bool ciclo_activo = false;

pthread_mutex_t mutex_instruccion = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_instruccion = PTHREAD_COND_INITIALIZER;
char* buffer_instruccion = NULL;
int op_code_instruccion = 0;
bool respuesta_instruccion_lista = false;


void* atender_ciclo(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex_ciclo);
        while (!ciclo_activo) {
            pthread_cond_wait(&cond_ciclo, &mutex_ciclo);
        }
        ciclo_activo = false;
        pthread_mutex_unlock(&mutex_ciclo);

        pthread_mutex_lock(&mutex_estado_cpu);
        while (cpu_tiene_proceso) {
            pthread_mutex_unlock(&mutex_estado_cpu);
            ciclo_instruccion();
            pthread_mutex_lock(&mutex_estado_cpu);
        }
        pthread_mutex_unlock(&mutex_estado_cpu);
    }
    return NULL;
}

//ciclo de instrucciones
void ciclo_instruccion(){
    
    //fetch
    char *linea = fetch();

    if (linea == NULL)
        return;
    
    //decode
    t_instruccion *instruccion = decode(linea);
    free(linea);

    if(instruccion == NULL)
    {
        pthread_mutex_lock(&mutex_estado_cpu);
        cpu_tiene_proceso = false;
        pid_actual = -1;
        pthread_mutex_unlock(&mutex_estado_cpu);
        return;
    }

    e_instruccion tipo = instruccion->tipo_instruccion;
    bool es_syscall = instruccion_es_syscall(tipo);
    bool es_exit = tipo == EXIT;

    //execute
    execute(instruccion);
    //PC + 1 si no fue modificado
    if(!fue_modificado_pc){
        avanzar_pc();
    }else{
        fue_modificado_pc = false;
    }

    if (es_syscall) {
        if (!es_exit) {
            liberar_cpu(false);
        } else {
            pthread_mutex_lock(&mutex_estado_cpu);
            cpu_tiene_proceso = false;
            pid_actual = -1;
            pthread_mutex_unlock(&mutex_estado_cpu);
        }
        liberar_instruccion(instruccion);
        return;
    }

    //check interrupt
    check_interrupt();

    liberar_instruccion(instruccion);
}

//fetch 
char *fetch(){
    pthread_mutex_lock(&mutex_estado_cpu);
    int pid = pid_actual;
    pthread_mutex_unlock(&mutex_estado_cpu);

    int pc = get_registro("PC");

    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, pc);

    solicitar_instruccion(pid, pc, socket_kernel_memory);

    pthread_mutex_lock(&mutex_instruccion);
    while (!respuesta_instruccion_lista) {
        pthread_cond_wait(&cond_instruccion, &mutex_instruccion);
    }
    respuesta_instruccion_lista = false;
    int op = op_code_instruccion;
    char* linea = buffer_instruccion;
    buffer_instruccion = NULL;
    pthread_mutex_unlock(&mutex_instruccion);

    if (op != INSTRUCCION) {
        log_error(logger,
                  "## PID: %d - Error al obtener instrucción desde Kernel Memory",
                  pid);
        enviar_error_cpu(pid, socket_kernel_scheduler);
        pthread_mutex_lock(&mutex_estado_cpu);
        cpu_tiene_proceso = false;
        pid_actual = -1;
        pthread_mutex_unlock(&mutex_estado_cpu);
        if (linea != NULL) free(linea);
        return NULL;
    }

    return linea;
}

t_instruccion* decode(char* linea) {
    // 1. Separamos el string por espacios
    // string_split nos devuelve un array terminado en NULL: ["SET", "BX", "25", NULL]
    char** tokens = string_split(linea, " ");
    
    // Si la línea estaba vacía, liberamos y salimos
    if (string_array_is_empty(tokens)) {
        string_array_destroy(tokens);
        return NULL;
    }

    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    
    // 2. Identificamos el tipo de instrucción usando el primer token
    instruccion->tipo_instruccion = string_a_enum(tokens[0]);

    // 3. Manejamos los parámetros
    // Calculamos cuántos parámetros hay (sin contar el comando)
    int total_tokens = string_array_size(tokens);
    int cant_parametros = total_tokens - 1;

    // Reservamos memoria para los punteros de los parámetros (+1 para el NULL al final)
    instruccion->parametros = malloc(sizeof(char*) * (cant_parametros + 1));

    for (int i = 0; i < cant_parametros; i++) {
        // Usamos string_duplicate para que la instrucción sea dueña de sus strings
        instruccion->parametros[i] = string_duplicate(tokens[i + 1]);
    }
    instruccion->parametros[cant_parametros] = NULL;

    // 4. Limpiamos el array temporal de tokens
    string_array_destroy(tokens);

    return instruccion;
}

//execute
void execute(t_instruccion* instruccion) {
    if (instruccion == NULL) return;
    
    // Obtenemos el tipo (que es un int que sirve de índice)
    e_instruccion tipo = instruccion->tipo_instruccion;
    char* parametros = parametros_a_string(instruccion->parametros);
    
    log_info(logger,
        "## PID: %d - Ejecutando: %s - %s",
        pid_actual,
        nombre_instruccion(tipo),
        parametros
    );
    
    free(parametros);
    
    // Invocamos la función correspondiente pasándole sus parámetros
    if (tipo < 0 || tipo > INSTRUCCION_INVALIDA || dispatch_table[tipo] == NULL) {
        instruccion_no_implementada(instruccion->parametros);
        return;
    }
    
    dispatch_table[tipo](instruccion->parametros);
}

void check_interrupt(void) {
    t_registro_interrupcion* interrupcion = NULL;

    pthread_mutex_lock(&mutex_lista_interrupciones);

    while (lista_interrupciones != NULL && !list_is_empty(lista_interrupciones)) {
        t_registro_interrupcion* candidata = list_remove(lista_interrupciones, 0);

        pthread_mutex_lock(&mutex_estado_cpu);
        bool coincide = cpu_tiene_proceso && candidata->pid == pid_actual;
        int pid_actual_log = pid_actual;
        pthread_mutex_unlock(&mutex_estado_cpu);

        if (coincide) {
            interrupcion = candidata;
            break;
        }

        log_debug(logger,
                  "Interrupción atrasada descartada para PID %d; PID actual: %d",
                  candidata->pid,
                  pid_actual_log);
        free(candidata);
    }

    pthread_mutex_unlock(&mutex_lista_interrupciones);

    if (interrupcion == NULL)
        return;

    int pid = interrupcion->pid;
    bool es_desalojo = interrupcion->es_desalojo;
    free(interrupcion);

    pthread_mutex_lock(&mutex_estado_cpu);
    if (!cpu_tiene_proceso || pid != pid_actual) {
        int pid_actual_log = pid_actual;
        pthread_mutex_unlock(&mutex_estado_cpu);
        log_debug(logger,
                  "Interrupción atrasada descartada para PID %d; PID actual: %d",
                  pid,
                  pid_actual_log);
        return;
    }

    if (es_desalojo) {
        pthread_mutex_unlock(&mutex_estado_cpu);
        liberar_cpu(true);
        log_info(logger, "CPU recibió interrupción para PID %d", pid);
        return;
    }

    pthread_mutex_lock(&mutex_estado_cpu);
    int old_pid = pid_actual;
    pthread_mutex_unlock(&mutex_estado_cpu);

    enviar_contexto_ejecucion(registros, socket_kernel_memory);
    enviar_cpu_desalojada(old_pid, socket_kernel_scheduler);

    pthread_mutex_lock(&mutex_estado_cpu);
    pid_actual = pid;
    pthread_mutex_unlock(&mutex_estado_cpu);

    log_info(logger, "CPU recibió interrupción para PID %d", pid);
    enviar_pedido_contexto(pid, socket_kernel_memory);
}




e_instruccion string_a_enum(char* comando) {
    // Usamos string_equals_ignore_case para mayor robustez
    
    // Instrucciones de CPU
    if (string_equals_ignore_case(comando, "NOOP"))      return NOOP;
    if (string_equals_ignore_case(comando, "SET"))       return SET;
    if (string_equals_ignore_case(comando, "MOV_IN"))    return MOV_IN;
    if (string_equals_ignore_case(comando, "MOV_OUT"))   return MOV_OUT;
    if (string_equals_ignore_case(comando, "SUM"))       return SUM;
    if (string_equals_ignore_case(comando, "SUB"))       return SUB;
    if (string_equals_ignore_case(comando, "JNZ"))       return JNZ;
    if (string_equals_ignore_case(comando, "COPY_MEM"))  return COPY_MEM;

    // Syscalls (Kernel)
    if (string_equals_ignore_case(comando, "MUTEX_CREATE")) return MUTEX_CREATE;
    if (string_equals_ignore_case(comando, "MUTEX_LOCK"))   return MUTEX_LOCK;
    if (string_equals_ignore_case(comando, "MUTEX_UNLOCK")) return MUTEX_UNLOCK;
    if (string_equals_ignore_case(comando, "MEM_ALLOC"))    return MEM_ALLOC;
    if (string_equals_ignore_case(comando, "MEM_FREE"))     return MEM_FREE;
    if (string_equals_ignore_case(comando, "SLEEP"))        return SLEEP;
    if (string_equals_ignore_case(comando, "STDIN"))        return STDIN;
    if (string_equals_ignore_case(comando, "STDOUT"))       return STDOUT;
    if (string_equals_ignore_case(comando, "INIT_PROC"))    return INIT_PROC;
    if (string_equals_ignore_case(comando, "EXIT"))         return EXIT;

    // Log de error opcional si no reconoce la instrucción
    // log_error(logger, "Instrucción desconocida: %s", comando);
    
    return INSTRUCCION_INVALIDA; 
}

void liberar_instruccion(t_instruccion* ins) {
    if (ins == NULL) return;
    
    // Usamos la lógica de la biblioteca para el array de parámetros
    // ya que es un array de char* terminado en NULL
    string_array_destroy(ins->parametros);
    free(ins);
}

void liberar_cpu(bool notificar_desalojo) {
    if (notificar_desalojo) {
        pthread_mutex_lock(&mutex_estado_cpu);
        int pid_actual_local = pid_actual;
        pthread_mutex_unlock(&mutex_estado_cpu);
        enviar_cpu_desalojada(pid_actual_local, socket_kernel_scheduler);
    }
    enviar_contexto_ejecucion(registros, socket_kernel_memory);
    pthread_mutex_lock(&mutex_estado_cpu);
    cpu_tiene_proceso = false;
    pid_actual = -1;
    pthread_mutex_unlock(&mutex_estado_cpu);
}


char* nombre_instruccion(e_instruccion tipo) {
    switch(tipo) {
        case NOOP: return "NOOP";
        case SET: return "SET";
        case MOV_IN: return "MOV_IN";
        case MOV_OUT: return "MOV_OUT";
        case SUM: return "SUM";
        case SUB: return "SUB";
        case JNZ: return "JNZ";
        case COPY_MEM: return "COPY_MEM";
        case MUTEX_CREATE: return "MUTEX_CREATE";
        case MUTEX_LOCK: return "MUTEX_LOCK";
        case MUTEX_UNLOCK: return "MUTEX_UNLOCK";
        case MEM_ALLOC: return "MEM_ALLOC";
        case MEM_FREE: return "MEM_FREE";
        case SLEEP: return "SLEEP";
        case STDIN: return "STDIN";
        case STDOUT: return "STDOUT";
        case INIT_PROC: return "INIT_PROC";
        case EXIT: return "EXIT";
        default: return "DESCONOCIDA";
    }
}

char* parametros_a_string(char** parametros) {
    char* texto = string_new();

    if(parametros == NULL || parametros[0] == NULL) {
        string_append(&texto, "-");
        return texto;
    }

    for(int i = 0; parametros[i] != NULL; i++) {
        if(i > 0)
            string_append(&texto, " ");

        string_append(&texto, parametros[i]);
    }

    return texto;
}
