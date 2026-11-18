#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <commons/log.h>
#include <cspecs/cspec.h>
#include "planificadores.h"
#include "algoritmos.h"
#include "config.h"
#include "init.h"
#include "types.h"

extern int conexion_memoria;

context(test_planificadores) {
    describe("comparar_prioridad_y_tiempo") {
        it("deberia ordenar por prioridad (menor = mas prioritario)") {
            t_pcb a = { .pid = 1, .prioridad = 5, .tiempo_suspendido = 100 };
            t_pcb b = { .pid = 2, .prioridad = 3, .tiempo_suspendido = 100 };

            should_bool(comparar_prioridad_y_tiempo(&a, &b)) be equal to(false);
            should_bool(comparar_prioridad_y_tiempo(&b, &a)) be equal to(true);
        } end

        it("deberia desempatar por tiempo_suspendido (mayor = mas viejo)") {
            t_pcb a = { .pid = 1, .prioridad = 3, .tiempo_suspendido = 50 };
            t_pcb b = { .pid = 2, .prioridad = 3, .tiempo_suspendido = 100 };

            should_bool(comparar_prioridad_y_tiempo(&a, &b)) be equal to(false);
            should_bool(comparar_prioridad_y_tiempo(&b, &a)) be equal to(true);
        } end

        it("deberia devolver false si son iguales") {
            t_pcb a = { .pid = 1, .prioridad = 3, .tiempo_suspendido = 100 };
            t_pcb b = { .pid = 2, .prioridad = 3, .tiempo_suspendido = 100 };

            should_bool(comparar_prioridad_y_tiempo(&a, &b)) be equal to(false);
        } end
    } end

    describe("pasar_a_exit") {
        before {
            logger = log_create("test_planificadores.log", "TEST", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_exit, NULL);
            cola_exit = list_create();
            conexion_memoria = -1;
        } end

        after {
            list_destroy(cola_exit);
            cola_exit = NULL;
            pthread_mutex_destroy(&mutex_exit);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia mover el PCB a EXIT") {
            t_pcb* pcb = malloc(sizeof(t_pcb));
            pcb->pid = 99;
            pcb->estado = READY;

            pasar_a_exit(pcb);

            should_int(pcb->estado) be equal to(EXIT);
            should_int(list_size(cola_exit)) be equal to(1);
            should_int(((t_pcb*)list_get(cola_exit, 0))->pid) be equal to(99);

            free(pcb);
        } end
    } end

    describe("vaciar_lista") {
        before {
            logger = log_create("test_planificadores.log", "TEST", false, LOG_LEVEL_DEBUG);
        } end

        after {
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia liberar todos los elementos de una lista") {
            t_list* lista = list_create();

            t_pcb* a = malloc(sizeof(t_pcb));
            a->pid = 1;
            t_pcb* b = malloc(sizeof(t_pcb));
            b->pid = 2;
            list_add(lista, a);
            list_add(lista, b);

            vaciar_lista(lista);

            should_int(list_size(lista)) be equal to(0);
            list_destroy(lista);
        } end
    } end
}
