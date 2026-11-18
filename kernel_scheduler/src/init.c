#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <utils/comunicacion.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include "algoritmos.h"
#include "config.h"
#include "init.h"
#include "memory_cliente.h"
#include "planificadores.h"
#include "servidor.h"
#include "mutex.h"
#include "types.h"


// =========================
// VARIABLES GLOBALES REALES
// =========================

// PLANIFICACIÓN
t_algoritmo_planificacion algoritmo_largo_plazo = ALGORITMO_FIFO;
t_algoritmo_planificacion algoritmo_corto_plazo = ALGORITMO_FIFO;

int cantidad_colas_cmn = 0;
t_list** colas_cmn = NULL;
t_algoritmo_cola* algoritmos_colas_cmn = NULL;

// CPUs
t_list* cpus_disponibles = NULL;
pthread_mutex_t mutex_cpus;
int proximo_cpu_id = 0;

// IO
t_list* ios_conectadas = NULL;
pthread_mutex_t mutex_ios;

//VARIABLES GLOBALES
//==================================================
extern t_log* logger;
extern t_config* config;
bool compactacion_en_progreso = false;
pthread_mutex_t mutex_compactacion;

// COLAS DE PLANIFICACIÓN (variables globales)
t_list* cola_new;
t_list* cola_ready;
t_list* cola_exec;
t_list* cola_block;
t_list* cola_exit;

t_list* cola_susp_block;
t_list* cola_susp_ready;

t_list* lista_block_timers;

//  MUTEX
pthread_mutex_t mutex_new;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_exec;
pthread_mutex_t mutex_block;

pthread_mutex_t mutex_block_timers;
pthread_mutex_t mutex_susp_block;
pthread_mutex_t mutex_susp_ready;

pthread_mutex_t mutex_exit;

static int contar_algoritmos_colas(char** algoritmos) {
    int cantidad = 0;

    if (algoritmos == NULL) {
        return 0;
    }

    while (algoritmos[cantidad] != NULL) {
        cantidad++;
    }

    return cantidad;
}

static t_algoritmo_cola parsear_algoritmo_cola(char* valor) {
    if (valor == NULL) {
        return COLA_FIFO;
    }

    if (strcasecmp(valor, "FIFO") == 0) {
        return COLA_FIFO;
    }

    if (strcasecmp(valor, "RR") == 0) {
        return COLA_RR;
    }

    log_warning(logger, "Algoritmo de cola CMN desconocido '%s', se usa FIFO", valor);
    return COLA_FIFO;
}

static void inicializar_colas_cmn(void) {
    cantidad_colas_cmn = contar_algoritmos_colas(QUEUES_ALGORITHMS);

    if (cantidad_colas_cmn == 0) {
        log_error(logger, "CMN requiere QUEUES_ALGORITHMS con al menos una cola");
        exit(EXIT_FAILURE);
    }

    colas_cmn = malloc(sizeof(t_list*) * cantidad_colas_cmn);
    algoritmos_colas_cmn = malloc(sizeof(t_algoritmo_cola) * cantidad_colas_cmn);

    if (colas_cmn == NULL || algoritmos_colas_cmn == NULL) {
        log_error(logger, "No se pudo reservar memoria para colas CMN");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < cantidad_colas_cmn; i++) {
        colas_cmn[i] = list_create();
        algoritmos_colas_cmn[i] = parsear_algoritmo_cola(QUEUES_ALGORITHMS[i]);

        log_debug(logger, "Cola CMN prioridad %d inicializada con %s",
            i,
            algoritmos_colas_cmn[i] == COLA_RR ? "RR" : "FIFO"
        );
    }

    log_debug(logger, "## Colas CMN inicializadas - Cantidad: %d", cantidad_colas_cmn);
}

// =========================
// INICIO DEL MODULO
//==================================================
void iniciar_modulo(char* path_config, char* path_proceso_inicial) {

    // 1. CONFIG
    config = iniciar_config(path_config);
    
    //Block timer 
    lista_block_timers = list_create();

    // 2. Cargar config global
    cargar_configuraciones(config);

    // 3. LOGGER
    iniciar_logger();
    inicializar_algoritmos_planificacion();
    // 4. Loggerar config 
    loggear_config();

    // MUTEX INIT (IMPORTANTE)
    pthread_mutex_init(&mutex_new, NULL);
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_exit, NULL);
    pthread_mutex_init(&mutex_cpus, NULL);
    pthread_mutex_init(&mutex_ios, NULL);

    pthread_mutex_init(&mutex_block_timers, NULL);
    pthread_mutex_init(&mutex_susp_block, NULL);
    pthread_mutex_init(&mutex_susp_ready, NULL);
    
    pthread_mutex_init(&mutex_compactacion, NULL);

    // INICIALIZAR COLAS
    cola_new = list_create();
    cola_ready = list_create();
    cola_exec = list_create();
    cola_block = list_create();
    cola_exit = list_create();
    cpus_disponibles = list_create();
    ios_conectadas = list_create();

    cola_susp_block = list_create();
    cola_susp_ready = list_create();

    inicializar_mutexes();

    // INICIALIZACIÓN CMN

    if (algoritmo_corto_plazo == ALGORITMO_CMN) {
        inicializar_colas_cmn();
    }

    // 5. CONEXIÓN A MEMORIA
    conectar_con_memoria();

    // PROCESO INICIAL
    // =========================
    //Proceso inicial (PID 0) = NEW
    if (!memoria_crear_proceso(0, path_proceso_inicial)) {
        exit(EXIT_FAILURE);
    }

    t_pcb* proceso_inicial = malloc(sizeof(t_pcb));
    proceso_inicial->pid = 0;
    proceso_inicial->estado = NEW;
    proceso_inicial->prioridad = 0;
    proceso_inicial->prioridad_original = 0;
    proceso_inicial->tiempo_suspendido = 0;
    proceso_inicial->intentos_memoria = 0;

    list_add(cola_new, proceso_inicial);
    log_info(logger, "## (0) Se crea el proceso - Estado: NEW");

    //6. PLANIFICADOR DE LARGO PLAZO (acordate arriba esta declarada pero comentada)
    pthread_t hilo_largo_plazo;
    
    if (pthread_create(&hilo_largo_plazo, NULL, planificador_largo_plazo, NULL) != 0) {
        log_error(logger, "Error creando hilo de largo plazo");
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_largo_plazo);

    //7. Planificador de CORTO PLAZO 
    inicializar_planificador_corto_plazo();

    //8. Planificadore de MEDIANO PLAZO 
    pthread_t hilo_mediano_plazo;

    if (pthread_create(&hilo_mediano_plazo, NULL, planificador_mediano_plazo, NULL) != 0) {
        log_error(logger, "Error creando hilo mediano plazo");
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_mediano_plazo);

/*
    // SIMULADOR BSOD (TESTING)
    // =========================
    pthread_t hilo_bsod;

    log_info(logger, "Creando simulador BSOD...");

    if (pthread_create(&hilo_bsod, NULL, simulador_bsod, NULL) != 0) {
        log_error(logger, "Error creando hilo BSOD");
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_bsod);

    log_info(logger, "Simulador BSOD activo (TEST)");
    //HASTA ACA ES LA SIMULACION
*/

    // 7. SERVIDOR
    int server_fd = iniciar_servidor(PUERTO_ESCUCHA);
    log_debug(logger, "Servidor escuchando...");

    // 8. ACEPTAR CLIENTES INFINITO (LOOPS DE CLIENTES)
    while (1) {

        int cliente_fd = esperar_cliente(server_fd);//esperar_clientes ya esta en utils

        pthread_t hilo;

        int* fd = malloc(sizeof(int));
        *fd = cliente_fd;

        pthread_create(&hilo, NULL, atender_cliente, fd);
        pthread_detach(hilo);
    }

    config_destroy(config);
    log_destroy(logger);

}
