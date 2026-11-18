#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "globals.h"
#include "registros.h"
#include <cspecs/cspec.h>

context(test_registros) {
    describe("Registros") {
        before {
            registros = malloc(sizeof(t_registro));
            memset(registros, 0, sizeof(t_registro));
            fue_modificado_pc = false;
        } end

        after {
            free(registros);
            registros = NULL;
        } end

        it("deberia setear y obtener AX") {
            set_registro("AX", 200);
            should_int(get_registro("AX")) be equal to(200);
        } end

        it("deberia truncar AX a 8 bits") {
            set_registro("AX", 256);
            should_int(get_registro("AX")) be equal to(0);
        } end

        it("deberia setear y obtener BX") {
            set_registro("BX", 100);
            should_int(get_registro("BX")) be equal to(100);
        } end

        it("deberia setear y obtener CX") {
            set_registro("CX", 50);
            should_int(get_registro("CX")) be equal to(50);
        } end

        it("deberia setear y obtener DX") {
            set_registro("DX", 1);
            should_int(get_registro("DX")) be equal to(1);
        } end

        it("deberia setear y obtener EAX") {
            set_registro("EAX", 70000);
            should_int(get_registro("EAX")) be equal to(70000);
        } end

        it("deberia setear y obtener EBX") {
            set_registro("EBX", 1000);
            should_int(get_registro("EBX")) be equal to(1000);
        } end

        it("deberia setear y obtener ECX") {
            set_registro("ECX", 99999);
            should_int(get_registro("ECX")) be equal to(99999);
        } end

        it("deberia setear y obtener EDX") {
            set_registro("EDX", 0);
            should_int(get_registro("EDX")) be equal to(0);
        } end

        it("deberia setear y obtener SI") {
            set_registro("SI", 42);
            should_int(get_registro("SI")) be equal to(42);
        } end

        it("deberia setear y obtener DI") {
            set_registro("DI", 123);
            should_int(get_registro("DI")) be equal to(123);
        } end

        it("deberia setear y obtener PC") {
            set_registro("PC", 15);
            should_int(get_registro("PC")) be equal to(15);
        } end

        it("deberia marcar fue_modificado_pc al setear PC") {
            set_registro("PC", 5);
            should_bool(fue_modificado_pc) be equal to(true);
        } end

        it("NO deberia marcar fue_modificado_pc al setear AX") {
            set_registro("AX", 10);
            should_bool(fue_modificado_pc) be equal to(false);
        } end

        it("deberia sumar dos registros de 8 bits") {
            set_registro("AX", 10);
            set_registro("BX", 20);
            sumar_registros("AX", "BX");
            should_int(get_registro("AX")) be equal to(30);
        } end

        it("deberia sumar dos registros de 32 bits") {
            set_registro("EAX", 1000);
            set_registro("EBX", 500);
            sumar_registros("EAX", "EBX");
            should_int(get_registro("EAX")) be equal to(1500);
        } end

        it("deberia restar dos registros") {
            set_registro("EAX", 100);
            set_registro("EBX", 30);
            restar_registros("EAX", "EBX");
            should_int(get_registro("EAX")) be equal to(70);
        } end

        it("deberia restar y dar negativo (wrap 32 bits)") {
            set_registro("EAX", 10);
            set_registro("EBX", 20);
            restar_registros("EAX", "EBX");
            should_int(get_registro("EAX")) be equal to((uint32_t)-10);
        } end

        it("JNZ no deberia saltar si registro es 0") {
            set_registro("AX", 0);
            set_registro("PC", 0);
            fue_modificado_pc = false;
            jnz_registros("AX", 99);
            should_bool(fue_modificado_pc) be equal to(false);
            should_int(get_registro("PC")) be equal to(0);
        } end

        it("JNZ deberia saltar si registro no es 0") {
            set_registro("AX", 1);
            set_registro("PC", 0);
            fue_modificado_pc = false;
            jnz_registros("AX", 99);
            should_bool(fue_modificado_pc) be equal to(true);
            should_int(get_registro("PC")) be equal to(99);
        } end

        it("avanzar_pc deberia incrementar PC en 1") {
            set_registro("PC", 0);
            avanzar_pc();
            should_int(get_registro("PC")) be equal to(1);
        } end

        it("avanzar_pc deberia incrementar PC desde cualquier valor") {
            set_registro("PC", 42);
            avanzar_pc();
            should_int(get_registro("PC")) be equal to(43);
        } end

        it("deberia tratar nombres de registro case-insensitive") {
            set_registro("ax", 200);
            should_int(get_registro("AX")) be equal to(200);
            set_registro("eax", 5000);
            should_int(get_registro("EAX")) be equal to(5000);
        } end

        it("deberia devolver 0 para registro desconocido") {
            should_int(get_registro("INVALIDO")) be equal to(0);
        } end

        it("deberia mezclar registros de 8 y 32 bits") {
            set_registro("AX", 5);
            set_registro("BX", 3);
            set_registro("EAX", 100);
            sumar_registros("EAX", "AX");
            should_int(get_registro("EAX")) be equal to(105);
            should_int(get_registro("AX")) be equal to(5);
        } end
    } end
}
