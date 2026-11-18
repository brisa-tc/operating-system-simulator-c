#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <commons/log.h>
#include "conexion.h"
#include <cspecs/cspec.h>

// Definidos en main.c (excluido de tests), necesarios por conexion.o
t_log* logger;
void* memoria;
int tamanio_memoria;
int memory_delay;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

context(test_memory_stick) {
    describe("validar_acceso") {
        it("deberia aceptar acceso a todo el bloque desde direccion 0") {
            should_bool(validar_acceso(0, 100, 100)) be equal to(true);
        } end

        it("deberia aceptar acceso al primer byte") {
            should_bool(validar_acceso(0, 1, 100)) be equal to(true);
        } end

        it("deberia aceptar acceso al ultimo byte") {
            should_bool(validar_acceso(99, 1, 100)) be equal to(true);
        } end

        it("deberia aceptar tamanio 0 (no-op)") {
            should_bool(validar_acceso(0, 0, 100)) be equal to(true);
        } end

        it("deberia rechazar direccion negativa") {
            should_bool(validar_acceso(-1, 1, 100)) be equal to(false);
        } end

        it("deberia rechazar tamanio negativo") {
            should_bool(validar_acceso(0, -1, 100)) be equal to(false);
        } end

        it("deberia rechazar direccion mayor que tamanio_memoria") {
            should_bool(validar_acceso(101, 1, 100)) be equal to(false);
        } end

        it("deberia rechazar direccion igual a tamanio_memoria con tamanio > 0") {
            should_bool(validar_acceso(100, 1, 100)) be equal to(false);
        } end

        it("deberia rechazar acceso que excede el limite") {
            should_bool(validar_acceso(50, 51, 100)) be equal to(false);
        } end

        it("deberia aceptar acceso justo en el limite") {
            should_bool(validar_acceso(50, 50, 100)) be equal to(true);
        } end

        it("deberia aceptar direccion igual a tamanio con tamanio 0") {
            should_bool(validar_acceso(100, 0, 100)) be equal to(true);
        } end

        it("deberia manejar memoria de tamanio 0") {
            should_bool(validar_acceso(0, 0, 0)) be equal to(true);
        } end

        it("deberia rechazar lectura en memoria de tamanio 0") {
            should_bool(validar_acceso(0, 1, 0)) be equal to(false);
        } end
    } end

    describe("escribir_memoria_stick y leer_memoria_stick") {
        before {
            memoria = calloc(1, 64);
            tamanio_memoria = 64;
            mutex_memoria = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
        } end

        after {
            free(memoria);
            memoria = NULL;
        } end

        it("deberia escribir y leer 1 byte en direccion 0") {
            uint8_t valor = 0xAB;
            escribir_memoria_stick(memoria, 0, &valor, 1, &mutex_memoria);

            uint8_t leido;
            leer_memoria_stick(memoria, 0, &leido, 1, &mutex_memoria);
            should_int(leido) be equal to(0xAB);
        } end

        it("deberia escribir y leer un bloque completo") {
            char* datos = "Hola Mundo!";
            escribir_memoria_stick(memoria, 0, datos, strlen(datos) + 1, &mutex_memoria);

            char buffer[20] = {0};
            leer_memoria_stick(memoria, 0, buffer, strlen(datos) + 1, &mutex_memoria);
            should_string(buffer) be equal to("Hola Mundo!");
        } end

        it("deberia escribir y leer en una direccion distinta de 0") {
            char* datos = "XYZ";
            escribir_memoria_stick(memoria, 10, datos, strlen(datos) + 1, &mutex_memoria);

            char buffer[20] = {0};
            leer_memoria_stick(memoria, 10, buffer, strlen(datos) + 1, &mutex_memoria);
            should_string(buffer) be equal to("XYZ");
        } end

        it("no deberia modificar otras posiciones al escribir") {
            char datos_antes[] = {0xFF, 0xFF, 0xFF};
            escribir_memoria_stick(memoria, 0, datos_antes, 3, &mutex_memoria);

            char dato_medio = 0x00;
            escribir_memoria_stick(memoria, 1, &dato_medio, 1, &mutex_memoria);

            char buffer[3];
            leer_memoria_stick(memoria, 0, buffer, 3, &mutex_memoria);
            should_int((uint8_t)buffer[0]) be equal to(0xFF);
            should_int((uint8_t)buffer[1]) be equal to(0x00);
            should_int((uint8_t)buffer[2]) be equal to(0xFF);
        } end

        it("deberia llenar toda la memoria y leer correctamente") {
            for (int i = 0; i < 64; i++) {
                uint8_t val = (uint8_t)i;
                escribir_memoria_stick(memoria, i, &val, 1, &mutex_memoria);
            }

            for (int i = 0; i < 64; i++) {
                uint8_t leido;
                leer_memoria_stick(memoria, i, &leido, 1, &mutex_memoria);
                should_int(leido) be equal to(i);
            }
        } end

        it("deberia escribir y leer en el ultimo byte disponible") {
            uint8_t valor = 0x42;
            escribir_memoria_stick(memoria, 63, &valor, 1, &mutex_memoria);

            uint8_t leido;
            leer_memoria_stick(memoria, 63, &leido, 1, &mutex_memoria);
            should_int(leido) be equal to(0x42);
        } end

        it("deberia ser thread-safe con acceso concurrente") {
            pthread_mutex_t mutex_test = PTHREAD_MUTEX_INITIALIZER;
            char buffer_escritura[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

            escribir_memoria_stick(memoria, 0, buffer_escritura, 10, &mutex_test);

            char buffer_lectura[10];
            leer_memoria_stick(memoria, 0, buffer_lectura, 10, &mutex_test);

            for (int i = 0; i < 10; i++) {
                should_int((uint8_t)buffer_lectura[i]) be equal to((uint8_t)buffer_escritura[i]);
            }
        } end
    } end
}
