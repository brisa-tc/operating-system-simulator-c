#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "globals.h"
#include "conexiones.h"
#include "MMU.h"
#include "utils/contexto_utils.h"
#include "utils/memorySticks.h"
#include <cspecs/cspec.h>

context(test_mmu) {
    describe("traducir_rango_logico") {
        before {
            logger = log_create("/tmp/cpu_test.log", "test", false, LOG_LEVEL_DEBUG);
            tam_max_segmento_cpu = 256;
            tabla_segmentos_actual = list_create();

            t_segmento_contexto* seg0 = malloc(sizeof(t_segmento_contexto));
            seg0->id = 0;
            seg0->base = 100;
            seg0->tamanio = 128;
            list_add(tabla_segmentos_actual, seg0);

            t_segmento_contexto* seg1 = malloc(sizeof(t_segmento_contexto));
            seg1->id = 1;
            seg1->base = 500;
            seg1->tamanio = 256;
            list_add(tabla_segmentos_actual, seg1);
        } end

        after {
            if (tabla_segmentos_actual != NULL) {
                list_destroy_and_destroy_elements(tabla_segmentos_actual, free);
                tabla_segmentos_actual = NULL;
            }
            if (logger != NULL) {
                log_destroy(logger);
                logger = NULL;
            }
        } end

        it("deberia traducir dir_logica 0 a dir_fisica igual a base del segmento 0") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(0, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_OK);
            should_int(dir_fisica) be equal to(100);
        } end

        it("deberia traducir dir_logica con desplazamiento dentro del segmento 0") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(50, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_OK);
            should_int(dir_fisica) be equal to(150);
        } end

        it("deberia traducir dir_logica que cae en segmento 1") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(256, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_OK);
            should_int(dir_fisica) be equal to(500);
        } end

        it("deberia traducir dir_logica con desplazamiento dentro del segmento 1") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(300, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_OK);
            should_int(dir_fisica) be equal to(544);
        } end

        it("deberia devolver SEG_FAULT si el acceso excede el tamanio del segmento") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(0, 200, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_SEG_FAULT);
        } end

        it("deberia devolver SEG_FAULT si el numero de segmento no existe") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(512, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_SEG_FAULT);
        } end

        it("deberia devolver SEG_FAULT si la tabla de segmentos es NULL") {
            list_destroy_and_destroy_elements(tabla_segmentos_actual, free);
            tabla_segmentos_actual = NULL;

            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(0, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_SEG_FAULT);
        } end

        it("deberia devolver SEG_FAULT si tamanio es negativo") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(0, -1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_SEG_FAULT);
        } end

        it("deberia permitir acceso justo en el ultimo byte del segmento") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(127, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_OK);
            should_int(dir_fisica) be equal to(227);
        } end

        it("deberia hacer SEG_FAULT al acceder justo despues del ultimo byte del segmento") {
            uint32_t dir_fisica;
            t_resultado_memoria_cpu res = traducir_rango_logico(128, 1, &dir_fisica);
            should_int(res) be equal to(MEM_CPU_SEG_FAULT);
        } end
    } end

    describe("obtener_info_ms_por_direccion_fisica") {
        before {
            logger = log_create("/tmp/cpu_test.log", "test", false, LOG_LEVEL_DEBUG);
            lista_memory_sticks = list_create();

            t_memoryStick_info* ms1 = malloc(sizeof(t_memoryStick_info));
            ms1->base = 0;
            ms1->tamanio = 256;
            ms1->socket = 1;
            ms1->ip = strdup("127.0.0.1");
            ms1->puerto = strdup("5001");
            list_add(lista_memory_sticks, ms1);

            t_memoryStick_info* ms2 = malloc(sizeof(t_memoryStick_info));
            ms2->base = 256;
            ms2->tamanio = 256;
            ms2->socket = 2;
            ms2->ip = strdup("127.0.0.1");
            ms2->puerto = strdup("5002");
            list_add(lista_memory_sticks, ms2);
        } end

        after {
            if (lista_memory_sticks != NULL) {
                eliminar_lista_memory_sticks(lista_memory_sticks);
                lista_memory_sticks = NULL;
            }
            if (logger != NULL) {
                log_destroy(logger);
                logger = NULL;
            }
        } end

        it("deberia encontrar el primer memory stick para dir_fisica 0") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(0, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(true);
            should_int(socket) be equal to(1);
            should_int(base) be equal to(0);
            should_int(tamanio) be equal to(256);
        } end

        it("deberia encontrar el primer memory stick para dir_fisica dentro de su rango") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(100, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(true);
            should_int(socket) be equal to(1);
        } end

        it("deberia encontrar el primer memory stick en el ultimo byte de su rango") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(255, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(true);
            should_int(socket) be equal to(1);
        } end

        it("deberia encontrar el segundo memory stick para dir_fisica 256") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(256, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(true);
            should_int(socket) be equal to(2);
            should_int(base) be equal to(256);
            should_int(tamanio) be equal to(256);
        } end

        it("deberia encontrar el segundo memory stick para dir_fisica dentro de su rango") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(400, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(true);
            should_int(socket) be equal to(2);
        } end

        it("no deberia encontrar memory stick fuera de todos los rangos") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(999, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(false);
        } end

        it("no deberia encontrar memory stick exactamente despues del ultimo") {
            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(512, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(false);
        } end

        it("no deberia encontrar memory stick si la lista esta vacia") {
            eliminar_lista_memory_sticks(lista_memory_sticks);
            lista_memory_sticks = list_create();

            int socket;
            uint32_t base;
            int tamanio;
            bool encontrado = obtener_info_ms_por_direccion_fisica(0, &socket, &base, &tamanio);
            should_bool(encontrado) be equal to(false);

            list_destroy(lista_memory_sticks);
            lista_memory_sticks = NULL;
        } end
    } end
}
