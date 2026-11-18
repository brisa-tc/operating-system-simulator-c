#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <commons/log.h>
#include <cspecs/cspec.h>
#include "algoritmos.h"
#include "planificadores.h"
#include "config.h"
#include "types.h"

context(test_algoritmos) {
    describe("config_es_true") {
        it("deberia devolver true para TRUE") {
            should_bool(config_es_true("TRUE")) be equal to(true);
        } end

        it("deberia devolver true para true (minusculas, case-insensitive)") {
            should_bool(config_es_true("true")) be equal to(true);
        } end

        it("deberia devolver false para FALSE") {
            should_bool(config_es_true("FALSE")) be equal to(false);
        } end

        it("deberia devolver false para NULL") {
            should_bool(config_es_true(NULL)) be equal to(false);
        } end
    } end

    describe("nombre_estado") {
        it("deberia devolver NEW") {
            should_string(nombre_estado(NEW)) be equal to("NEW");
        } end

        it("deberia devolver READY") {
            should_string(nombre_estado(READY)) be equal to("READY");
        } end

        it("deberia devolver EXEC") {
            should_string(nombre_estado(EXEC)) be equal to("EXEC");
        } end

        it("deberia devolver BLOCK") {
            should_string(nombre_estado(BLOCK)) be equal to("BLOCK");
        } end

        it("deberia devolver SUSP. BLOCK") {
            should_string(nombre_estado(SUSP_BLOCK)) be equal to("SUSP. BLOCK");
        } end

        it("deberia devolver SUSP. READY") {
            should_string(nombre_estado(SUSP_READY)) be equal to("SUSP. READY");
        } end

        it("deberia devolver EXIT") {
            should_string(nombre_estado(EXIT)) be equal to("EXIT");
        } end
    } end

    describe("cambiar_estado") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
        } end

        after {
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia cambiar el estado del PCB") {
            t_pcb pcb;
            pcb.pid = 1;
            pcb.estado = NEW;

            cambiar_estado(&pcb, READY);

            should_int(pcb.estado) be equal to(READY);
        } end
    } end

    describe("hay_proceso_ready") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_ready, NULL);
            cola_ready = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
        } end

        after {
            list_destroy(cola_ready);
            cola_ready = NULL;
            pthread_mutex_destroy(&mutex_ready);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia devolver false cuando la cola esta vacia") {
            should_bool(hay_proceso_ready()) be equal to(false);
        } end

        it("deberia devolver true cuando hay procesos") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 42;
            list_add(cola_ready, pcb);

            should_bool(hay_proceso_ready()) be equal to(true);

            free(pcb);
        } end
    } end

    describe("agregar_a_ready") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_ready, NULL);
            pthread_mutex_init(&mutex_cpus, NULL);
            cola_ready = list_create();
            cpus_disponibles = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
            QUEUE_PREEMPTION = "FALSE";
        } end

        after {
            list_destroy(cola_ready);
            cola_ready = NULL;
            list_destroy(cpus_disponibles);
            cpus_disponibles = NULL;
            pthread_mutex_destroy(&mutex_ready);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia agregar un PCB a la cola de ready") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 1;
            pcb->prioridad = 0;
            pcb->estado = NEW;

            bool resultado = agregar_a_ready(pcb, NEW);

            should_bool(resultado) be equal to(true);
            should_int(pcb->estado) be equal to(READY);
            should_int(list_size(cola_ready)) be equal to(1);

            free(pcb);
        } end

        it("deberia devolver false para NULL") {
            should_bool(agregar_a_ready(NULL, NEW)) be equal to(false);
        } end
    } end

    describe("agregar_a_ready_al_principio") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_ready, NULL);
            cola_ready = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
        } end

        after {
            list_destroy(cola_ready);
            cola_ready = NULL;
            pthread_mutex_destroy(&mutex_ready);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia agregar al principio de la cola") {
            t_pcb* pcb1 = malloc(sizeof(t_pcb));
            pcb1->pid = 1;
            pcb1->prioridad = 0;
            list_add(cola_ready, pcb1);

            t_pcb* pcb2 = malloc(sizeof(t_pcb));
            pcb2->pid = 2;
            pcb2->prioridad = 0;

            agregar_a_ready_al_principio(pcb2, NEW);
            t_pcb* primero = list_get(cola_ready, 0);

            should_int(primero->pid) be equal to(2);
            should_int(list_size(cola_ready)) be equal to(2);

            free(pcb1);
            free(pcb2);
        } end

        it("deberia devolver false para NULL") {
            should_bool(agregar_a_ready_al_principio(NULL, NEW)) be equal to(false);
        } end
    } end

    describe("seleccionar_siguiente_ready") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_ready, NULL);
            cola_ready = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
        } end

        after {
            list_destroy(cola_ready);
            cola_ready = NULL;
            pthread_mutex_destroy(&mutex_ready);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia devolver el primer PCB de la cola") {
            t_pcb* pcb1 = malloc(sizeof(t_pcb));
            pcb1->pid = 10;
            t_pcb* pcb2 = malloc(sizeof(t_pcb));
            pcb2->pid = 20;
            list_add(cola_ready, pcb1);
            list_add(cola_ready, pcb2);

            t_algoritmo_cola alg;
            t_pcb* seleccionado = seleccionar_siguiente_ready(&alg);

            should_ptr(seleccionado) be equal to(pcb1);
            should_int(list_size(cola_ready)) be equal to(1);

            free(pcb1);
            free(pcb2);
        } end

        it("deberia devolver NULL para cola vacia") {
            t_algoritmo_cola alg;
            should_ptr(seleccionar_siguiente_ready(&alg)) be null;
        } end
    } end

    describe("desalojar_exec_a_ready") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_exec, NULL);
            pthread_mutex_init(&mutex_ready, NULL);
            pthread_mutex_init(&mutex_cpus, NULL);
            cola_exec = list_create();
            cola_ready = list_create();
            cpus_disponibles = list_create();
            algoritmo_corto_plazo = ALGORITMO_FIFO;
            QUEUE_PREEMPTION = "FALSE";
        } end

        after {
            list_destroy(cola_exec);
            cola_exec = NULL;
            list_destroy(cola_ready);
            cola_ready = NULL;
            list_destroy(cpus_disponibles);
            cpus_disponibles = NULL;
            pthread_mutex_destroy(&mutex_exec);
            pthread_mutex_destroy(&mutex_ready);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia mover un PCB de EXEC a READY") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 7;
            pcb->prioridad = 0;
            pcb->estado = EXEC;
            list_add(cola_exec, pcb);

            bool resultado = desalojar_exec_a_ready(pcb);

            should_bool(resultado) be equal to(true);
            should_int(pcb->estado) be equal to(READY);
            should_int(list_size(cola_exec)) be equal to(0);
            should_int(list_size(cola_ready)) be equal to(1);

            free(pcb);
        } end

        it("deberia devolver false para NULL") {
            should_bool(desalojar_exec_a_ready(NULL)) be equal to(false);
        } end
    } end

    describe("reservar_y_liberar_cpu") {
        before {
            logger = log_create("test_algoritmos.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_cpus, NULL);
            cpus_disponibles = list_create();
        } end

        after {
            list_destroy(cpus_disponibles);
            cpus_disponibles = NULL;
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia reservar una CPU libre") {
            t_cpu_kernel* cpu = malloc(sizeof(t_cpu_kernel));
            cpu->id = 0;
            cpu->socket_dispatch = 1;
            cpu->socket_interrupt = 2;
            cpu->libre = true;
            cpu->pcb = NULL;
            list_add(cpus_disponibles, cpu);

            t_cpu_kernel* reservada = reservar_cpu_libre();

            should_ptr(reservada) be equal to(cpu);
            should_bool(cpu->libre) be equal to(false);

            free(cpu);
        } end

        it("deberia devolver NULL si no hay CPUs libres") {
            should_ptr(reservar_cpu_libre()) be null;
        } end

        it("liberar_cpu deberia marcar la CPU como libre") {
            t_cpu_kernel* cpu = malloc(sizeof(t_cpu_kernel));
            cpu->id = 0;
            cpu->socket_dispatch = 1;
            cpu->socket_interrupt = 2;
            cpu->libre = false;
            cpu->pcb = NULL;
            list_add(cpus_disponibles, cpu);

            liberar_cpu(cpu);

            should_bool(cpu->libre) be equal to(true);

            free(cpu);
        } end
    } end
}
