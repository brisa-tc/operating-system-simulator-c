#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ciclo_instruccion.h"
#include <cspecs/cspec.h>

context(test_decodificacion) {
    describe("string_a_enum") {
        it("deberia reconocer NOOP") {
            should_int(string_a_enum("NOOP")) be equal to(NOOP);
        } end

        it("deberia reconocer SET") {
            should_int(string_a_enum("SET")) be equal to(SET);
        } end

        it("deberia reconocer MOV_IN") {
            should_int(string_a_enum("MOV_IN")) be equal to(MOV_IN);
        } end

        it("deberia reconocer MOV_OUT") {
            should_int(string_a_enum("MOV_OUT")) be equal to(MOV_OUT);
        } end

        it("deberia reconocer SUM") {
            should_int(string_a_enum("SUM")) be equal to(SUM);
        } end

        it("deberia reconocer SUB") {
            should_int(string_a_enum("SUB")) be equal to(SUB);
        } end

        it("deberia reconocer JNZ") {
            should_int(string_a_enum("JNZ")) be equal to(JNZ);
        } end

        it("deberia reconocer COPY_MEM") {
            should_int(string_a_enum("COPY_MEM")) be equal to(COPY_MEM);
        } end

        it("deberia reconocer MUTEX_CREATE") {
            should_int(string_a_enum("MUTEX_CREATE")) be equal to(MUTEX_CREATE);
        } end

        it("deberia reconocer MUTEX_LOCK") {
            should_int(string_a_enum("MUTEX_LOCK")) be equal to(MUTEX_LOCK);
        } end

        it("deberia reconocer MUTEX_UNLOCK") {
            should_int(string_a_enum("MUTEX_UNLOCK")) be equal to(MUTEX_UNLOCK);
        } end

        it("deberia reconocer MEM_ALLOC") {
            should_int(string_a_enum("MEM_ALLOC")) be equal to(MEM_ALLOC);
        } end

        it("deberia reconocer MEM_FREE") {
            should_int(string_a_enum("MEM_FREE")) be equal to(MEM_FREE);
        } end

        it("deberia reconocer SLEEP") {
            should_int(string_a_enum("SLEEP")) be equal to(SLEEP);
        } end

        it("deberia reconocer STDIN") {
            should_int(string_a_enum("STDIN")) be equal to(STDIN);
        } end

        it("deberia reconocer STDOUT") {
            should_int(string_a_enum("STDOUT")) be equal to(STDOUT);
        } end

        it("deberia reconocer INIT_PROC") {
            should_int(string_a_enum("INIT_PROC")) be equal to(INIT_PROC);
        } end

        it("deberia reconocer EXIT") {
            should_int(string_a_enum("EXIT")) be equal to(EXIT);
        } end

        it("deberia devolver INSTRUCCION_INVALIDA para comando desconocido") {
            should_int(string_a_enum("FOOBAR")) be equal to(INSTRUCCION_INVALIDA);
        } end

        it("deberia ser case-insensitive") {
            should_int(string_a_enum("set")) be equal to(SET);
            should_int(string_a_enum("Set")) be equal to(SET);
            should_int(string_a_enum("noop")) be equal to(NOOP);
        } end
    } end

    describe("nombre_instruccion") {
        it("deberia devolver el nombre correcto para NOOP") {
            should_string(nombre_instruccion(NOOP)) be equal to("NOOP");
        } end

        it("deberia devolver el nombre correcto para EXIT") {
            should_string(nombre_instruccion(EXIT)) be equal to("EXIT");
        } end

        it("deberia devolver DESCONOCIDA para valor invalido") {
            should_string(nombre_instruccion(999)) be equal to("DESCONOCIDA");
        } end

        it("deberia devolver el nombre correcto para INSTRUCCION_INVALIDA") {
            should_string(nombre_instruccion(INSTRUCCION_INVALIDA)) be equal to("DESCONOCIDA");
        } end
    } end

    describe("decode") {
        it("deberia decodificar SET AX 25") {
            t_instruccion* ins = decode("SET AX 25");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(SET);
            should_string(ins->parametros[0]) be equal to("AX");
            should_string(ins->parametros[1]) be equal to("25");
            should_ptr(ins->parametros[2]) be null;
            liberar_instruccion(ins);
        } end

        it("deberia decodificar SUM BX CX") {
            t_instruccion* ins = decode("SUM BX CX");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(SUM);
            should_string(ins->parametros[0]) be equal to("BX");
            should_string(ins->parametros[1]) be equal to("CX");
            should_ptr(ins->parametros[2]) be null;
            liberar_instruccion(ins);
        } end

        it("deberia decodificar NOOP sin parametros") {
            t_instruccion* ins = decode("NOOP");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(NOOP);
            should_ptr(ins->parametros[0]) be null;
            liberar_instruccion(ins);
        } end

        it("deberia decodificar JNZ AX 10") {
            t_instruccion* ins = decode("JNZ AX 10");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(JNZ);
            should_string(ins->parametros[0]) be equal to("AX");
            should_string(ins->parametros[1]) be equal to("10");
            liberar_instruccion(ins);
        } end

        it("deberia devolver INSTRUCCION_INVALIDA para linea vacia") {
            t_instruccion* ins = decode("");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(INSTRUCCION_INVALIDA);
            should_ptr(ins->parametros[0]) be null;
            liberar_instruccion(ins);
        } end

        it("deberia devolver INSTRUCCION_INVALIDA para comando desconocido") {
            t_instruccion* ins = decode("FOOBAR");
            should_ptr(ins) not be null;
            should_int(ins->tipo_instruccion) be equal to(INSTRUCCION_INVALIDA);
            liberar_instruccion(ins);
        } end
    } end

    describe("parametros_a_string") {
        it("deberia concatenar parametros") {
            char* params[] = {"AX", "25", NULL};
            char* resultado = parametros_a_string(params);
            should_string(resultado) be equal to("AX 25");
            free(resultado);
        } end

        it("deberia devolver '-' para array NULL") {
            char* resultado = parametros_a_string(NULL);
            should_string(resultado) be equal to("-");
            free(resultado);
        } end

        it("deberia devolver '-' para array vacio") {
            char* params[] = {NULL};
            char* resultado = parametros_a_string(params);
            should_string(resultado) be equal to("-");
            free(resultado);
        } end

        it("deberia manejar un solo parametro") {
            char* params[] = {"NOOP", NULL};
            char* resultado = parametros_a_string(params);
            should_string(resultado) be equal to("NOOP");
            free(resultado);
        } end

        it("deberia manejar multiples parametros") {
            char* params[] = {"param1", "param2", "param3", NULL};
            char* resultado = parametros_a_string(params);
            should_string(resultado) be equal to("param1 param2 param3");
            free(resultado);
        } end
    } end

    describe("liberar_instruccion") {
        it("no deberia crashear con NULL") {
            liberar_instruccion(NULL);
        } end
    } end
}
