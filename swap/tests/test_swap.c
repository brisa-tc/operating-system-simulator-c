#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <commons/log.h>
#include "swap.h"
#include <cspecs/cspec.h>

// Definido en main.c (excluido de tests), necesario por comunicacion.h
t_log* logger;

context(test_swap) {
    describe("bloque_valido") {
        it("deberia aceptar bloque 0") {
            should_bool(bloque_valido(0, 10)) be equal to(true);
        } end

        it("deberia aceptar el ultimo bloque") {
            should_bool(bloque_valido(9, 10)) be equal to(true);
        } end

        it("deberia rechazar bloque negativo") {
            should_bool(bloque_valido(-1, 10)) be equal to(false);
        } end

        it("deberia rechazar bloque igual a block_count") {
            should_bool(bloque_valido(10, 10)) be equal to(false);
        } end

        it("deberia rechazar bloque con block_count 0") {
            should_bool(bloque_valido(0, 0)) be equal to(false);
        } end

        it("deberia rechazar bloque negativo con block_count 0") {
            should_bool(bloque_valido(-5, 10)) be equal to(false);
        } end
    } end

    describe("direccion_swap_valida") {
        it("deberia aceptar acceso a todo el archivo desde direccion 0") {
            should_bool(direccion_swap_valida(0, 100, 100)) be equal to(true);
        } end

        it("deberia aceptar tamanio 0") {
            should_bool(direccion_swap_valida(0, 0, 100)) be equal to(true);
        } end

        it("deberia aceptar acceso en archivo de tamanio 0 con tamanio 0") {
            should_bool(direccion_swap_valida(0, 0, 0)) be equal to(true);
        } end

        it("deberia rechazar direccion negativa") {
            should_bool(direccion_swap_valida(-1, 1, 100)) be equal to(false);
        } end

        it("deberia rechazar acceso que excede el tamanio del archivo") {
            should_bool(direccion_swap_valida(0, 101, 100)) be equal to(false);
        } end

        it("deberia rechazar acceso parcialmente fuera de rango") {
            should_bool(direccion_swap_valida(50, 51, 100)) be equal to(false);
        } end

        it("deberia aceptar acceso justo en el limite") {
            should_bool(direccion_swap_valida(50, 50, 100)) be equal to(true);
        } end

        it("deberia rechazar acceso empezando justo despues del final") {
            should_bool(direccion_swap_valida(100, 1, 100)) be equal to(false);
        } end

        it("deberia aceptar direccion igual a swap_file_size con tamanio 0") {
            should_bool(direccion_swap_valida(100, 0, 100)) be equal to(true);
        } end
    } end
}
