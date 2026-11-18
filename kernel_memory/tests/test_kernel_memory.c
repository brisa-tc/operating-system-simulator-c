#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include "estructura.h"
#include "procesos.h"
#include "segmentos.h"
#include "registro.h"
#include <cspecs/cspec.h>

static t_hueco* crear_hueco(uint32_t base, uint32_t tamanio)
{
    t_hueco* h = malloc(sizeof(t_hueco));
    h->base = base;
    h->tamanio = tamanio;
    return h;
}

static t_memoryStick_info* crear_ms(uint32_t base, int tamanio)
{
    t_memoryStick_info* ms = malloc(sizeof(t_memoryStick_info));
    ms->base = base;
    ms->tamanio = tamanio;
    ms->socket = -1;
    ms->ip = NULL;
    ms->puerto = NULL;
    return ms;
}

static t_contexto* ctx_global;
static t_segmento* seg_global_0;
static t_segmento* seg_global_1;

context(test_kernel_memory) {
    describe("iniciar_registros") {
        it("deberia setear todos los registros a 0") {
            t_registros reg;
            memset(&reg, 0xFF, sizeof(reg));
            iniciar_registros(&reg);
            should_int(reg.PC)  be equal to(0);
            should_int(reg.AX)  be equal to(0);
            should_int(reg.BX)  be equal to(0);
            should_int(reg.CX)  be equal to(0);
            should_int(reg.DX)  be equal to(0);
            should_int(reg.EAX) be equal to(0);
            should_int(reg.EBX) be equal to(0);
            should_int(reg.ECX) be equal to(0);
            should_int(reg.EDX) be equal to(0);
            should_int(reg.SI)  be equal to(0);
            should_int(reg.DI)  be equal to(0);
        } end
    } end

    describe("bitmap_swap") {
        before {
            logger = log_create("/tmp/km_bitmap_test.log", "test", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_swap, NULL);
        } end

        after {
            if (bitmap_swap != NULL) {
                bitarray_destroy(bitmap_swap);
                bitmap_swap = NULL;
            }
            free(bitmap_swap_storage);
            bitmap_swap_storage = NULL;
            swap_block_count = 0;
            swap_block_size = 0;
            swap_file_size = 0;
            pthread_mutex_destroy(&mutex_swap);
            log_destroy(logger);
            logger = NULL;
        } end

        it("inicializar_bitmap_swap 16/128 deberia crear 8 bloques") {
            inicializar_bitmap_swap(16, 128);
            should_int(swap_block_count) be equal to(8);
            should_ptr(bitmap_swap) not be null;
        } end

        it("inicializar_bitmap_swap con block_size=0 no deberia crear bitmap") {
            inicializar_bitmap_swap(0, 128);
            should_ptr(bitmap_swap) be null;
        } end

        it("inicializar_bitmap_swap con file_size=0 no deberia crear bitmap") {
            inicializar_bitmap_swap(16, 0);
            should_ptr(bitmap_swap) be null;
        } end

        it("obtener_bloque_swap_libre deberia devolver el primer bloque") {
            inicializar_bitmap_swap(16, 128);
            int bloque = obtener_bloque_swap_libre();
            should_int(bloque) be equal to(0);
        } end

        it("deberia saltar bloques ocupados al buscar libre") {
            inicializar_bitmap_swap(16, 128);
            marcar_bloque_swap_ocupado(0);
            int bloque = obtener_bloque_swap_libre();
            should_int(bloque) be equal to(1);
        } end

        it("marcar_bloque_swap_libre deberia re-disponibilizar un bloque") {
            inicializar_bitmap_swap(16, 128);
            marcar_bloque_swap_ocupado(0);
            marcar_bloque_swap_ocupado(1);
            marcar_bloque_swap_libre(1);
            int bloque = obtener_bloque_swap_libre();
            should_int(bloque) be equal to(1);
        } end

        it("obtener_bloque_swap_libre deberia devolver -1 cuando todos ocupados") {
            inicializar_bitmap_swap(16, 128);
            for (int i = 0; i < 8; i++)
                marcar_bloque_swap_ocupado(i);
            int bloque = obtener_bloque_swap_libre();
            should_int(bloque) be equal to(-1);
        } end

        it("hay_bloques_swap_disponibles deberia devolver true si hay suficientes") {
            inicializar_bitmap_swap(16, 128);
            should_bool(hay_bloques_swap_disponibles(5)) be equal to(true);
        } end

        it("hay_bloques_swap_disponibles deberia devolver false si no hay suficientes") {
            inicializar_bitmap_swap(16, 128);
            for (int i = 0; i < 7; i++)
                marcar_bloque_swap_ocupado(i);
            should_bool(hay_bloques_swap_disponibles(2)) be equal to(false);
        } end

        it("marcar_bloque_swap_ocupado con indice invalido no deberia fallar") {
            inicializar_bitmap_swap(16, 128);
            marcar_bloque_swap_ocupado(999);
            should_int(obtener_bloque_swap_libre()) be equal to(0);
        } end
    } end

    describe("buscar_best_fit") {
        before {
            logger = log_create("/tmp/km_fit_test.log", "test", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_huecos, NULL);
            lista_huecos = list_create();

            list_add(lista_huecos, crear_hueco(0,   50));
            list_add(lista_huecos, crear_hueco(100, 200));
            list_add(lista_huecos, crear_hueco(400, 30));
            list_add(lista_huecos, crear_hueco(500, 150));
        } end

        after {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = NULL;
            pthread_mutex_destroy(&mutex_huecos);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia encontrar el hueco mas chico que entre (30 entre 50/200/30/150)") {
            t_hueco* h = buscar_best_fit(30);
            should_ptr(h) not be null;
            should_int(h->base) be equal to(400);
            should_int(h->tamanio) be equal to(30);
        } end

        it("deberia devolver NULL si ningun hueco entra") {
            t_hueco* h = buscar_best_fit(999);
            should_ptr(h) be null;
        } end

        it("deberia devolver NULL si la lista de huecos esta vacia") {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            should_ptr(buscar_best_fit(10)) be null;
        } end

        it("deberia preferir el hueco de tamanio exacto sobre uno mas grande") {
            list_add(lista_huecos, crear_hueco(700, 25));
            t_hueco* h = buscar_best_fit(25);
            should_ptr(h) not be null;
            should_int(h->base) be equal to(700);
            should_int(h->tamanio) be equal to(25);
        } end
    } end

    describe("buscar_worst_fit") {
        before {
            logger = log_create("/tmp/km_wfit_test.log", "test", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_huecos, NULL);
            lista_huecos = list_create();

            list_add(lista_huecos, crear_hueco(0,   50));
            list_add(lista_huecos, crear_hueco(100, 200));
            list_add(lista_huecos, crear_hueco(400, 30));
        } end

        after {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = NULL;
            pthread_mutex_destroy(&mutex_huecos);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia encontrar el hueco mas grande que entre (200 entre 50/200/30)") {
            t_hueco* h = buscar_worst_fit(30);
            should_ptr(h) not be null;
            should_int(h->base) be equal to(100);
            should_int(h->tamanio) be equal to(200);
        } end

        it("deberia devolver NULL si ningun hueco entra") {
            should_ptr(buscar_worst_fit(999)) be null;
        } end

        it("deberia devolver NULL si la lista de huecos esta vacia") {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            should_ptr(buscar_worst_fit(10)) be null;
        } end
    } end

    describe("obtener_espacio_libre") {
        before {
            logger = log_create("/tmp/km_space_test.log", "test", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_huecos, NULL);
            lista_huecos = list_create();
        } end

        after {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = NULL;
            pthread_mutex_destroy(&mutex_huecos);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia devolver 0 cuando no hay huecos") {
            should_int(obtener_espacio_libre()) be equal to(0);
        } end

        it("deberia sumar el tamanio de todos los huecos") {
            list_add(lista_huecos, crear_hueco(0, 100));
            list_add(lista_huecos, crear_hueco(200, 50));
            list_add(lista_huecos, crear_hueco(300, 75));
            should_int(obtener_espacio_libre()) be equal to(225);
        } end
    } end

    describe("obtener_memoria_total") {
        before {
            pthread_mutex_init(&mutex_memory_sticks, NULL);
            lista_memory_sticks = list_create();
        } end

        after {
            list_destroy_and_destroy_elements(lista_memory_sticks, free);
            lista_memory_sticks = NULL;
            pthread_mutex_destroy(&mutex_memory_sticks);
        } end

        it("deberia devolver 0 cuando no hay memory sticks") {
            should_int(obtener_memoria_total()) be equal to(0);
        } end

        it("deberia sumar el tamanio de todos los memory sticks") {
            list_add(lista_memory_sticks, crear_ms(0, 256));
            list_add(lista_memory_sticks, crear_ms(256, 128));
            list_add(lista_memory_sticks, crear_ms(384, 64));
            should_int(obtener_memoria_total()) be equal to(448);
        } end
    } end

    describe("crear_contexto_y_buscar") {
        before {
            logger = log_create("/tmp/km_ctx_test.log", "test", false, LOG_LEVEL_DEBUG);
            pthread_mutex_init(&mutex_contextos, NULL);
            lista_contextos = list_create();
        } end

        after {
            for (int i = list_size(lista_contextos) - 1; i >= 0; i--) {
                t_contexto* ctx = list_remove(lista_contextos, i);
                list_destroy(ctx->tabla_segmentos->segmentos);
                free(ctx->tabla_segmentos);
                list_destroy_and_destroy_elements(ctx->segmentos_en_swap, free);
                free(ctx->path_instrucciones);
                free(ctx);
            }
            list_destroy(lista_contextos);
            lista_contextos = NULL;
            pthread_mutex_destroy(&mutex_contextos);
            log_destroy(logger);
            logger = NULL;
        } end

        it("crear_contexto deberia crear un contexto con el pid dado") {
            t_contexto* ctx = crear_contexto(42, "/tmp/test.prc");
            should_ptr(ctx) not be null;
            should_int(ctx->pid) be equal to(42);
        } end

        it("crear_contexto deberia inicializar tabla de segmentos vacia") {
            t_contexto* ctx = crear_contexto(1, "/tmp/test.prc");
            should_ptr(ctx->tabla_segmentos) not be null;
            should_int(list_size(ctx->tabla_segmentos->segmentos)) be equal to(0);
        } end

        it("buscar_contexto deberia encontrar el contexto creado") {
            t_contexto* creado = crear_contexto(99, "/tmp/test.prc");
            t_contexto* encontrado = buscar_contexto(99);
            should_ptr(encontrado) not be null;
            should_ptr(encontrado) be equal to(creado);
        } end

        it("buscar_contexto deberia devolver NULL para pid inexistente") {
            should_ptr(buscar_contexto(999)) be null;
        } end
    } end

    describe("traducir_direccion") {
        before {
            logger = log_create("/tmp/km_trans_test.log", "test", false, LOG_LEVEL_DEBUG);
            iniciar_estructuras();
            segment_max_size = 128;

            ctx_global = crear_contexto(1, "/tmp/test.prc");
            allocation_strategy = strdup("BEST");

            pthread_mutex_lock(&mutex_huecos);
            list_add(lista_huecos, crear_hueco(0, 256));
            pthread_mutex_unlock(&mutex_huecos);

            crear_segmento(ctx_global, 0, 100);
            crear_segmento(ctx_global, 1, 128);
        } end

        after {
            if (ctx_global != NULL) {
                while (!list_is_empty(ctx_global->tabla_segmentos->segmentos)) {
                    t_segmento* seg = list_get(ctx_global->tabla_segmentos->segmentos, 0);
                    eliminar_segmento(ctx_global, seg->id);
                }
                pthread_mutex_lock(&mutex_contextos);
                list_remove_element(lista_contextos, ctx_global);
                pthread_mutex_unlock(&mutex_contextos);
                list_destroy(ctx_global->tabla_segmentos->segmentos);
                free(ctx_global->tabla_segmentos);
                list_destroy_and_destroy_elements(ctx_global->segmentos_en_swap, free);
                free(ctx_global->path_instrucciones);
                free(ctx_global);
                ctx_global = NULL;
            }
            free(allocation_strategy);
            allocation_strategy = NULL;
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            pthread_mutex_destroy(&mutex_huecos);
            pthread_mutex_destroy(&mutex_segmentos);
            pthread_mutex_destroy(&mutex_segmentos_global);
            pthread_mutex_destroy(&mutex_contextos);
            pthread_mutex_destroy(&mutex_memory_sticks);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia traducir direccion logica 0 al inicio del segmento 0") {
            uint32_t dir_fisica;
            int res = traducir_direccion(1, 0, 1, &dir_fisica);
            should_int(res) be equal to(0);
            should_int(dir_fisica) be equal to(0);
        } end

        it("deberia traducir direccion logica con desplazamiento en segmento 0") {
            uint32_t dir_fisica;
            int res = traducir_direccion(1, 50, 1, &dir_fisica);
            should_int(res) be equal to(0);
            should_int(dir_fisica) be equal to(50);
        } end

        it("deberia traducir direccion logica en segmento 1") {
            uint32_t dir_fisica;
            int res = traducir_direccion(1, 128, 1, &dir_fisica);
            should_int(res) be equal to(0);
            should_int(dir_fisica) be equal to(100);
        } end

        it("deberia devolver -1 si el acceso excede el tamanio del segmento") {
            uint32_t dir_fisica;
            int res = traducir_direccion(1, 0, 200, &dir_fisica);
            should_int(res) be equal to(-1);
        } end

        it("deberia devolver -1 si el segmento no existe") {
            uint32_t dir_fisica;
            int res = traducir_direccion(1, 256, 1, &dir_fisica);
            should_int(res) be equal to(-1);
        } end

        it("deberia devolver -1 si el proceso no existe") {
            uint32_t dir_fisica;
            int res = traducir_direccion(999, 0, 1, &dir_fisica);
            should_int(res) be equal to(-1);
        } end
    } end

    describe("crear_segmento") {
        before {
            logger = log_create("/tmp/km_crearseg_test.log", "test", false, LOG_LEVEL_DEBUG);
            iniciar_estructuras();
            segment_max_size = 128;
            allocation_strategy = strdup("BEST");

            ctx_global = crear_contexto(1, "/tmp/test.prc");

            pthread_mutex_lock(&mutex_huecos);
            list_add(lista_huecos, crear_hueco(0, 256));
            pthread_mutex_unlock(&mutex_huecos);
        } end

        after {
            if (ctx_global != NULL) {
                while (!list_is_empty(ctx_global->tabla_segmentos->segmentos)) {
                    t_segmento* seg = list_get(ctx_global->tabla_segmentos->segmentos, 0);
                    eliminar_segmento(ctx_global, seg->id);
                }
                pthread_mutex_lock(&mutex_contextos);
                list_remove_element(lista_contextos, ctx_global);
                pthread_mutex_unlock(&mutex_contextos);
                list_destroy(ctx_global->tabla_segmentos->segmentos);
                free(ctx_global->tabla_segmentos);
                list_destroy_and_destroy_elements(ctx_global->segmentos_en_swap, free);
                free(ctx_global->path_instrucciones);
                free(ctx_global);
                ctx_global = NULL;
            }
            free(allocation_strategy);
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            pthread_mutex_destroy(&mutex_huecos);
            pthread_mutex_destroy(&mutex_segmentos);
            pthread_mutex_destroy(&mutex_segmentos_global);
            pthread_mutex_destroy(&mutex_contextos);
            pthread_mutex_destroy(&mutex_memory_sticks);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia crear un segmento con BEST FIT") {
            t_segmento* seg = crear_segmento(ctx_global, 0, 50);
            should_ptr(seg) not be null;
            should_int(seg->id) be equal to(0);
            should_int(seg->base) be equal to(0);
            should_int(seg->tamanio) be equal to(50);
        } end

        it("deberia rechazar un segmento con tamanio invalido") {
            should_ptr(crear_segmento(ctx_global, 0, 0)) be null;
            should_ptr(crear_segmento(ctx_global, 0, -1)) be null;
            should_ptr(crear_segmento(ctx_global, 0, 200)) be null;
        } end

        it("deberia rechazar un segmento con id duplicado") {
            crear_segmento(ctx_global, 0, 50);
            should_ptr(crear_segmento(ctx_global, 0, 30)) be null;
        } end

        it("deberia devolver NULL si no hay huecos disponibles") {
            pthread_mutex_lock(&mutex_huecos);
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            pthread_mutex_unlock(&mutex_huecos);
            should_ptr(crear_segmento(ctx_global, 0, 50)) be null;
        } end

        it("deberia usar WORST FIT si la estrategia es WORST") {
            free(allocation_strategy);
            allocation_strategy = strdup("WORST");

            pthread_mutex_lock(&mutex_huecos);
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            list_add(lista_huecos, crear_hueco(0, 30));
            list_add(lista_huecos, crear_hueco(100, 200));
            pthread_mutex_unlock(&mutex_huecos);

            t_segmento* seg = crear_segmento(ctx_global, 5, 50);
            should_ptr(seg) not be null;
            should_int(seg->base) be equal to(100);
        } end

    } end

    describe("eliminar_segmento") {
        before {
            logger = log_create("/tmp/km_delseg_test.log", "test", false, LOG_LEVEL_DEBUG);
            iniciar_estructuras();
            segment_max_size = 128;
            allocation_strategy = strdup("BEST");

            ctx_global = crear_contexto(1, "/tmp/test.prc");

            pthread_mutex_lock(&mutex_huecos);
            list_add(lista_huecos, crear_hueco(0, 256));
            pthread_mutex_unlock(&mutex_huecos);

            seg_global_0 = crear_segmento(ctx_global, 0, 80);
            seg_global_1 = crear_segmento(ctx_global, 1, 80);
        } end

        after {
            if (ctx_global != NULL) {
                while (!list_is_empty(ctx_global->tabla_segmentos->segmentos)) {
                    t_segmento* seg = list_get(ctx_global->tabla_segmentos->segmentos, 0);
                    eliminar_segmento(ctx_global, seg->id);
                }
                pthread_mutex_lock(&mutex_contextos);
                list_remove_element(lista_contextos, ctx_global);
                pthread_mutex_unlock(&mutex_contextos);
                list_destroy(ctx_global->tabla_segmentos->segmentos);
                free(ctx_global->tabla_segmentos);
                list_destroy_and_destroy_elements(ctx_global->segmentos_en_swap, free);
                free(ctx_global->path_instrucciones);
                free(ctx_global);
                ctx_global = NULL;
            }
            free(allocation_strategy);
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            pthread_mutex_destroy(&mutex_huecos);
            pthread_mutex_destroy(&mutex_segmentos);
            pthread_mutex_destroy(&mutex_segmentos_global);
            pthread_mutex_destroy(&mutex_contextos);
            pthread_mutex_destroy(&mutex_memory_sticks);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia crear un hueco al eliminar un segmento del medio") {
            int holes_before_remove = list_size(lista_huecos);
            eliminar_segmento(ctx_global, 0);
            should_int(list_size(lista_huecos)) be equal to(holes_before_remove + 1);
        } end

        it("deberia fusionar huecos adyacentes") {
            eliminar_segmento(ctx_global, 1);
            should_int(list_size(lista_huecos)) be equal to(1);
        } end

        it("eliminar segmento inexistente no deberia hacer nada") {
            int holes_before_remove = list_size(lista_huecos);
            eliminar_segmento(ctx_global, 999);
            should_int(list_size(lista_huecos)) be equal to(holes_before_remove);
        } end
    } end

    describe("ampliar_memoria_usuario") {
        before {
            logger = log_create("/tmp/km_ampliar_test.log", "test", false, LOG_LEVEL_DEBUG);
            iniciar_estructuras();
            segment_max_size = 128;
        } end

        after {
            list_destroy_and_destroy_elements(lista_huecos, free);
            lista_huecos = list_create();
            list_destroy_and_destroy_elements(lista_segmentos, free);
            lista_segmentos = list_create();
            pthread_mutex_destroy(&mutex_huecos);
            pthread_mutex_destroy(&mutex_segmentos);
            pthread_mutex_destroy(&mutex_segmentos_global);
            pthread_mutex_destroy(&mutex_contextos);
            pthread_mutex_destroy(&mutex_memory_sticks);
            pthread_mutex_destroy(&mutex_cpus);
            log_destroy(logger);
            logger = NULL;
        } end

        it("deberia agregar un hueco al final cuando no hay nada") {
            uint32_t base = ampliar_memoria_usuario(256);
            should_int(base) be equal to(0);
            should_int(list_size(lista_huecos)) be equal to(1);
            t_hueco* h = list_get(lista_huecos, 0);
            should_int(h->base) be equal to(0);
            should_int(h->tamanio) be equal to(256);
        } end

        it("deberia calcular base despues de segmentos existentes") {
            ampliar_memoria_usuario(100);
            ampliar_memoria_usuario(200);
            should_int(list_size(lista_huecos)) be equal to(2);
            t_hueco* h1 = list_get(lista_huecos, 0);
            t_hueco* h2 = list_get(lista_huecos, 1);
            should_int(h1->base) be equal to(0);
            should_int(h1->tamanio) be equal to(100);
            should_int(h2->base) be equal to(100);
            should_int(h2->tamanio) be equal to(200);
        } end
    } end
}
