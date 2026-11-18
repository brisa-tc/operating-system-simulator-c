#include "segmentos.h"
#include "procesos.h"
#include <stdlib.h>
#include <string.h>

// ─── Memoria total disponible ─────────────────────────────────────────────────

uint32_t obtener_memoria_total()
{
    uint32_t total = 0;
    pthread_mutex_lock(&mutex_memory_sticks);
    for (int i = 0; i < list_size(lista_memory_sticks); i++) {
        t_memoryStick_info* ms = list_get(lista_memory_sticks, i);
        total += ms->tamanio;
    }
    pthread_mutex_unlock(&mutex_memory_sticks);
    return total;
}

// ─── Best Fit ─────────────────────────────────────────────────────────────────
// Requiere lista_huecos bajo mutex_huecos (el caller lo toma)

t_hueco* buscar_best_fit(int tamanio)
{
    t_hueco* mejor = NULL;
    for (int i = 0; i < list_size(lista_huecos); i++) {
        t_hueco* h = list_get(lista_huecos, i);
        if ((int)h->tamanio >= tamanio) {
            if (mejor == NULL || h->tamanio < mejor->tamanio)
                mejor = h;
        }
    }
    return mejor;
}

// ─── Worst Fit ────────────────────────────────────────────────────────────────

t_hueco* buscar_worst_fit(int tamanio)
{
    t_hueco* peor = NULL;
    for (int i = 0; i < list_size(lista_huecos); i++) {
        t_hueco* h = list_get(lista_huecos, i);
        if ((int)h->tamanio >= tamanio) {
            if (peor == NULL || h->tamanio > peor->tamanio)
                peor = h;
        }
    }
    return peor;
}

// ─── Crear segmento ───────────────────────────────────────────────────────────

t_segmento* crear_segmento(t_contexto* ctx, int id, int tamanio)
{
    if (tamanio <= 0 || tamanio > segment_max_size) {
        log_warning(logger,
                    "## PID: %d - Segmento %d rechazado - Tamaño: %d - Max Size: %d",
                    ctx->pid,
                    id,
                    tamanio,
                    segment_max_size);
        return NULL;
    }
        // Verificar que no exista un segmento con el mismo id en el proceso
    
    pthread_mutex_lock(&mutex_segmentos);
    for (int i = 0; i < list_size(ctx->tabla_segmentos->segmentos); i++) {
        t_segmento* s = list_get(ctx->tabla_segmentos->segmentos, i);
        if (s->id == id) {
            pthread_mutex_unlock(&mutex_segmentos);
            return NULL;
        }
    }
    pthread_mutex_unlock(&mutex_segmentos);

    pthread_mutex_lock(&mutex_huecos);

    if (list_is_empty(lista_huecos)) {
        pthread_mutex_unlock(&mutex_huecos);
        return NULL;
    }

    t_hueco* hueco;
    if (strcmp(allocation_strategy, "BEST") == 0)
        hueco = buscar_best_fit(tamanio);
    else
        hueco = buscar_worst_fit(tamanio);

    if (hueco == NULL) {
        pthread_mutex_unlock(&mutex_huecos);
        return NULL;
    }

    t_segmento* nuevo = malloc(sizeof(t_segmento));
    if (nuevo == NULL) {
        pthread_mutex_unlock(&mutex_huecos);
        return NULL;
    }

    nuevo->id      = id;
    nuevo->base    = hueco->base;
    nuevo->tamanio = (uint32_t)tamanio;

    // Reducir el hueco
    hueco->base    += tamanio;
    hueco->tamanio -= tamanio;

    if (hueco->tamanio == 0) {
        list_remove_element(lista_huecos, hueco);
        free(hueco);
    }

    pthread_mutex_unlock(&mutex_huecos);

    // Agregar a lista global y a tabla del proceso
    pthread_mutex_lock(&mutex_segmentos_global);
    list_add(lista_segmentos, nuevo);
    pthread_mutex_unlock(&mutex_segmentos_global);

    pthread_mutex_lock(&mutex_segmentos);
    list_add(ctx->tabla_segmentos->segmentos, nuevo);
    pthread_mutex_unlock(&mutex_segmentos);

    log_info(logger, "## PID: %d - Segmento Creado %d - Tamaño: %d",
             ctx->pid, id, tamanio);
    return nuevo;
}

// ─── Eliminar segmento ────────────────────────────────────────────────────────

void eliminar_segmento(t_contexto *ctx, int id_segmento)
{
    t_segmento *seg = NULL;

    // Sacar de tabla del proceso
    pthread_mutex_lock(&mutex_segmentos);
    for (int i = 0; i < list_size(ctx->tabla_segmentos->segmentos); i++)
    {
        t_segmento *s = list_get(ctx->tabla_segmentos->segmentos, i);
        if (s->id == id_segmento)
        {
            seg = s;
            list_remove(ctx->tabla_segmentos->segmentos, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_segmentos);

    if (seg == NULL)
        return;

    // Sacar de lista global
    pthread_mutex_lock(&mutex_segmentos_global);
    list_remove_element(lista_segmentos, seg);
    pthread_mutex_unlock(&mutex_segmentos_global);

    // Devolver espacio como hueco
    pthread_mutex_lock(&mutex_huecos);
    t_hueco *nuevo_hueco = malloc(sizeof(t_hueco));
    nuevo_hueco->base = seg->base;
    nuevo_hueco->tamanio = seg->tamanio;
    list_add(lista_huecos, nuevo_hueco);
    pthread_mutex_unlock(&mutex_huecos);

    pthread_mutex_lock(&mutex_huecos);
    bool hubo_fusion = true;
    while (hubo_fusion)
    {
        hubo_fusion = false;
        for (int i = 0; i < list_size(lista_huecos); i++)
        {
            t_hueco *h1 = list_get(lista_huecos, i);
            for (int j = 0; j < list_size(lista_huecos); j++)
            {
                if (i == j)
                    continue;
                t_hueco *h2 = list_get(lista_huecos, j);
                if (h1->base + h1->tamanio == h2->base)
                {
                    h1->tamanio += h2->tamanio;
                    list_remove(lista_huecos, j);
                    free(h2);
                    hubo_fusion = true;
                    break;
                }
            }
            if (hubo_fusion)
                break;
        }
    }

    pthread_mutex_unlock(&mutex_huecos);

    log_info(logger, "## PID: %d - Segmento Eliminado %d", ctx->pid, id_segmento);
    free(seg);
}
