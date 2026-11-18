#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <commons/log.h>
#include "globals.h"
#include "instruccion.h"
#include <cspecs/cspec.h>

// Definido en main.c (excluido de tests), necesario por conexiones.c
int identificador;

static void setup_registros_y_logger(void) {
    registros = malloc(sizeof(t_registro));
    memset(registros, 0, sizeof(t_registro));
    fue_modificado_pc = false;
    logger = log_create("/tmp/cpu_test.log", "test", false, LOG_LEVEL_DEBUG);
}

static void teardown_registros_y_logger(void) {
    free(registros);
    registros = NULL;
    if (logger != NULL) {
        log_destroy(logger);
        logger = NULL;
    }
}

context(test_instrucciones) {
    describe("ejecutar_NOOP") {
        before {
            logger = log_create("/tmp/cpu_test.log", "test", false, LOG_LEVEL_DEBUG);
        } end

        after {
            if (logger != NULL) {
                log_destroy(logger);
                logger = NULL;
            }
        } end

        it("no deberia crashear") {
            char* params[] = {NULL};
            ejecutar_NOOP(params);
        } end
    } end

    describe("ejecutar_SET") {
        before {
            setup_registros_y_logger();
        } end

        after {
            teardown_registros_y_logger();
        } end

        it("deberia asignar valor a AX") {
            char* params[] = {"AX", "42", NULL};
            ejecutar_SET(params);
            should_int(get_registro("AX")) be equal to(42);
        } end

        it("deberia asignar valor a EAX") {
            char* params[] = {"EAX", "70000", NULL};
            ejecutar_SET(params);
            should_int(get_registro("EAX")) be equal to(70000);
        } end

        it("deberia asignar valor a PC") {
            char* params[] = {"PC", "10", NULL};
            ejecutar_SET(params);
            should_int(get_registro("PC")) be equal to(10);
        } end

        it("deberia marcar fue_modificado_pc al setear PC") {
            char* params[] = {"PC", "5", NULL};
            ejecutar_SET(params);
            should_bool(fue_modificado_pc) be equal to(true);
        } end
    } end

    describe("ejecutar_SUM") {
        before {
            setup_registros_y_logger();
            set_registro("AX", 10);
            set_registro("BX", 20);
            set_registro("EAX", 1000);
            set_registro("EBX", 500);
        } end

        after {
            teardown_registros_y_logger();
        } end

        it("deberia sumar dos registros de 8 bits") {
            char* params[] = {"AX", "BX", NULL};
            ejecutar_SUM(params);
            should_int(get_registro("AX")) be equal to(30);
        } end

        it("deberia sumar dos registros de 32 bits") {
            char* params[] = {"EAX", "EBX", NULL};
            ejecutar_SUM(params);
            should_int(get_registro("EAX")) be equal to(1500);
        } end
    } end

    describe("ejecutar_SUB") {
        before {
            setup_registros_y_logger();
            set_registro("EAX", 100);
            set_registro("EBX", 30);
        } end

        after {
            teardown_registros_y_logger();
        } end

        it("deberia restar dos registros") {
            char* params[] = {"EAX", "EBX", NULL};
            ejecutar_SUB(params);
            should_int(get_registro("EAX")) be equal to(70);
        } end

        it("deberia restar y dar resultado negativo") {
            set_registro("EAX", 10);
            set_registro("EBX", 20);
            char* params[] = {"EAX", "EBX", NULL};
            ejecutar_SUB(params);
            should_int(get_registro("EAX")) be equal to((uint32_t)-10);
        } end
    } end

    describe("ejecutar_JNZ") {
        before {
            setup_registros_y_logger();
        } end

        after {
            teardown_registros_y_logger();
        } end

        it("no deberia saltar si registro es 0") {
            set_registro("AX", 0);
            set_registro("PC", 0);
            fue_modificado_pc = false;
            char* params[] = {"AX", "99", NULL};
            ejecutar_JNZ(params);
            should_bool(fue_modificado_pc) be equal to(false);
            should_int(get_registro("PC")) be equal to(0);
        } end

        it("deberia saltar si registro no es 0") {
            set_registro("AX", 1);
            set_registro("PC", 0);
            fue_modificado_pc = false;
            char* params[] = {"AX", "99", NULL};
            ejecutar_JNZ(params);
            should_bool(fue_modificado_pc) be equal to(true);
            should_int(get_registro("PC")) be equal to(99);
        } end
    } end
}
