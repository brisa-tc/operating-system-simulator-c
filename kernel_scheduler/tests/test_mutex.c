#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <commons/log.h>
#include <cspecs/cspec.h>
#include "mutex.h"
#include "algoritmos.h"
#include "config.h"
#include "init.h"
#include "types.h"

extern t_list* lista_mutex;
t_log* logger;

context(test_mutex) {
    describe("mutex_create") {
        before {
            logger = log_create("test_mutex.log", "TEST", false, LOG_LEVEL_DEBUG);
            inicializar_mutexes();
        } end

        after {
            list_destroy_and_destroy_elements(lista_mutex, free);
            lista_mutex = NULL;
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia crear un nuevo mutex") {
            mutex_create("mutex_test");

            t_mutex_kernel* encontrado = buscar_mutex("mutex_test");

            should_ptr(encontrado) not be null;
            should_string(encontrado->nombre) be equal to("mutex_test");
            should_ptr(encontrado->duenio) be null;
        } end

        it("no deberia crear duplicados") {
            mutex_create("recurso");
            int size_antes = list_size(lista_mutex);

            mutex_create("recurso");

            should_int(list_size(lista_mutex)) be equal to(size_antes);
        } end

        it("no deberia hacer nada con nombre NULL") {
            int size_antes = list_size(lista_mutex);

            mutex_create(NULL);

            should_int(list_size(lista_mutex)) be equal to(size_antes);
        } end
    } end

    describe("buscar_mutex") {
        before {
            logger = log_create("test_mutex.log", "TEST", false, LOG_LEVEL_DEBUG);
            inicializar_mutexes();
            mutex_create("recurso_a");
            mutex_create("recurso_b");
        } end

        after {
            list_destroy_and_destroy_elements(lista_mutex, free);
            lista_mutex = NULL;
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia encontrar un mutex existente") {
            t_mutex_kernel* m = buscar_mutex("recurso_a");
            should_ptr(m) not be null;
            should_string(m->nombre) be equal to("recurso_a");
        } end

        it("deberia devolver NULL para un mutex inexistente") {
            should_ptr(buscar_mutex("no_existe")) be null;
        } end
    } end

    describe("mutex_lock") {
        before {
            logger = log_create("test_mutex.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_block, NULL);
            pthread_mutex_init(&mutex_ready, NULL);
            pthread_mutex_init(&mutex_block_timers, NULL);
            cola_block = list_create();
            cola_ready = list_create();
            lista_block_timers = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
            QUEUE_PREEMPTION = "FALSE";
            inicializar_mutexes();
            mutex_create("lock_test");
        } end

        after {
            list_destroy(cola_block);
            cola_block = NULL;
            list_destroy(cola_ready);
            cola_ready = NULL;
            list_destroy(lista_block_timers);
            lista_block_timers = NULL;
            list_destroy_and_destroy_elements(lista_mutex, free);
            lista_mutex = NULL;
            pthread_mutex_destroy(&mutex_block);
            pthread_mutex_destroy(&mutex_ready);
            pthread_mutex_destroy(&mutex_block_timers);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia tomar el lock si esta libre") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 1;
            pcb->prioridad = 5;
            pcb->prioridad_original = 5;

            bool resultado = mutex_lock(pcb, "lock_test");

            should_bool(resultado) be equal to(true);

            t_mutex_kernel* mutex = buscar_mutex("lock_test");
            should_ptr(mutex->duenio) be equal to(pcb);

            free(pcb);
        } end

        it("deberia bloquear al segundo proceso si el mutex esta tomado") {
            t_pcb* duenio = malloc(sizeof(t_pcb));
            duenio->pid = 1;
            duenio->prioridad = 5;
            duenio->prioridad_original = 5;

            t_pcb* segundo = malloc(sizeof(t_pcb));
            segundo->pid = 2;
            segundo->prioridad = 5;
            segundo->prioridad_original = 5;
            segundo->estado = READY;

            mutex_lock(duenio, "lock_test");
            bool resultado = mutex_lock(segundo, "lock_test");

            should_bool(resultado) be equal to(false);
            should_int(segundo->estado) be equal to(BLOCK);
            should_int(list_size(cola_block)) be equal to(1);

            free(duenio);
            free(segundo);
        } end

        it("deberia devolver true si el mismo proceso vuelve a lockear") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 1;
            pcb->prioridad = 5;

            mutex_lock(pcb, "lock_test");
            bool resultado = mutex_lock(pcb, "lock_test");

            should_bool(resultado) be equal to(true);

            free(pcb);
        } end
    } end

    describe("mutex_unlock") {
        before {
            logger = log_create("test_mutex.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_block, NULL);
            pthread_mutex_init(&mutex_ready, NULL);
            pthread_mutex_init(&mutex_block_timers, NULL);
            cola_block = list_create();
            cola_ready = list_create();
            lista_block_timers = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
            QUEUE_PREEMPTION = "FALSE";
            inicializar_mutexes();
            mutex_create("unlock_test");
        } end

        after {
            list_destroy(cola_block);
            cola_block = NULL;
            list_destroy(cola_ready);
            cola_ready = NULL;
            list_destroy(lista_block_timers);
            lista_block_timers = NULL;
            list_destroy_and_destroy_elements(lista_mutex, free);
            lista_mutex = NULL;
            pthread_mutex_destroy(&mutex_block);
            pthread_mutex_destroy(&mutex_ready);
            pthread_mutex_destroy(&mutex_block_timers);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia liberar el mutex si nadie esta esperando") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 1;
            pcb->prioridad = 5;
            pcb->prioridad_original = 5;

            mutex_lock(pcb, "unlock_test");
            mutex_unlock(pcb, "unlock_test");

            t_mutex_kernel* mutex = buscar_mutex("unlock_test");
            should_ptr(mutex->duenio) be null;

            free(pcb);
        } end

        it("deberia despertar al siguiente proceso bloqueado") {
            t_pcb* duenio = malloc(sizeof(t_pcb));
            duenio->pid = 1;
            duenio->prioridad = 5;
            duenio->prioridad_original = 5;

            t_pcb* segundo = malloc(sizeof(t_pcb));
            segundo->pid = 2;
            segundo->prioridad = 5;
            segundo->prioridad_original = 5;
            segundo->estado = READY;

            mutex_lock(duenio, "unlock_test");
            mutex_lock(segundo, "unlock_test");
            mutex_unlock(duenio, "unlock_test");

            t_mutex_kernel* mutex = buscar_mutex("unlock_test");
            should_ptr(mutex->duenio) be equal to(segundo);
            should_int(list_size(cola_block)) be equal to(0);
            should_int(segundo->estado) be equal to(READY);

            free(duenio);
            free(segundo);
        } end
    } end
}
