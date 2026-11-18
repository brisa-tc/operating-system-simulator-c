#include "mutex.h"
#include "init.h"
#include "algoritmos.h"

#include <commons/collections/list.h>
#include <commons/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

extern t_log* logger;

extern t_list* cola_ready;
extern t_list* cola_block;
extern t_list* cola_susp_block;
extern t_list* cola_susp_ready;
extern t_list* lista_block_timers;

extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_block;
extern pthread_mutex_t mutex_susp_block;
extern pthread_mutex_t mutex_susp_ready;
extern pthread_mutex_t mutex_block_timers;

t_list* lista_mutex;
static pthread_mutex_t mutex_lista_mutex;

static void registrar_timer_block_mutex(t_pcb* pcb) {
    t_block_entry* entry = malloc(sizeof(t_block_entry));
    if (entry == NULL) {
        log_error(logger, "No se pudo reservar timer de BLOCK para PID %d", pcb->pid);
        return;
    }

    entry->pcb = pcb;
    entry->inicio_block = time(NULL);

    pthread_mutex_lock(&mutex_block_timers);
    list_add(lista_block_timers, entry);
    pthread_mutex_unlock(&mutex_block_timers);
}

static void eliminar_timer_block_mutex(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_block_timers);

    for (int i = 0; i < list_size(lista_block_timers); i++) {
        t_block_entry* entry = list_get(lista_block_timers, i);

        if (entry->pcb == pcb) {
            list_remove(lista_block_timers, i);
            free(entry);
            break;
        }
    }

    pthread_mutex_unlock(&mutex_block_timers);
}

static void heredar_prioridad_si_corresponde(t_mutex_kernel* mutex, t_pcb* bloqueado) {
    t_pcb* duenio = mutex->duenio;

    if (duenio == NULL || bloqueado == NULL) {
        return;
    }

    if (bloqueado->prioridad < duenio->prioridad) {
        int prioridad_anterior = duenio->prioridad;
        duenio->prioridad = bloqueado->prioridad;

        log_info(logger,
            "## %d Cambio de prioridad: %d - %d",
            duenio->pid,
            prioridad_anterior,
            duenio->prioridad
        );
    }
}

static void aplicar_herencia_desde_bloqueados(t_mutex_kernel* mutex) {
    if (mutex == NULL || mutex->duenio == NULL) {
        return;
    }

    for (int i = 0; i < list_size(mutex->bloqueados); i++) {
        t_pcb* bloqueado = list_get(mutex->bloqueados, i);
        heredar_prioridad_si_corresponde(mutex, bloqueado);
    }
}

static int prioridad_mas_alta_bloqueada(t_mutex_kernel* mutex) {
    int prioridad = mutex->duenio->prioridad_original;

    for (int i = 0; i < list_size(mutex->bloqueados); i++) {
        t_pcb* bloqueado = list_get(mutex->bloqueados, i);

        if (bloqueado->prioridad < prioridad) {
            prioridad = bloqueado->prioridad;
        }
    }

    return prioridad;
}

static void recalcular_prioridad_por_mutexes(t_pcb* pcb) {
    if (pcb == NULL) {
        return;
    }

    int prioridad_nueva = pcb->prioridad_original;

    for (int i = 0; i < list_size(lista_mutex); i++) {
        t_mutex_kernel* mutex = list_get(lista_mutex, i);

        if (mutex->duenio == pcb) {
            int prioridad_mutex = prioridad_mas_alta_bloqueada(mutex);

            if (prioridad_mutex < prioridad_nueva) {
                prioridad_nueva = prioridad_mutex;
            }
        }
    }

    if (pcb->prioridad == prioridad_nueva) {
        return;
    }

    int prioridad_anterior = pcb->prioridad;
    pcb->prioridad = prioridad_nueva;

    log_info(logger,
        "## %d Cambio de prioridad: %d - %d",
        pcb->pid,
        prioridad_anterior,
        pcb->prioridad
    );
}

void inicializar_mutexes() {

    lista_mutex = list_create();
    pthread_mutex_init(&mutex_lista_mutex, NULL);
}



//BUSCAR MUTEX: Sirve para encontrar un mutex por nombre.
t_mutex_kernel* buscar_mutex(char* nombre) {
    if (nombre == NULL || lista_mutex == NULL) {
        return NULL;
    }

    for (int i = 0; i < list_size(lista_mutex); i++) {

        t_mutex_kernel* mutex = list_get(lista_mutex, i);

        if (strcmp(mutex->nombre, nombre) == 0) {
            return mutex;
        }
    }

    return NULL;
}

void mutex_create(char* nombre) {
    if (nombre == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex_lista_mutex);

    if (buscar_mutex(nombre) != NULL) {
        pthread_mutex_unlock(&mutex_lista_mutex);
        return;
    }

    t_mutex_kernel* nuevo_mutex = malloc(sizeof(t_mutex_kernel));
    if (nuevo_mutex == NULL) {
        pthread_mutex_unlock(&mutex_lista_mutex);
        log_error(logger, "No se pudo reservar mutex %s", nombre);
        return;
    }

    nuevo_mutex->nombre = strdup(nombre);
    nuevo_mutex->duenio = NULL;
    nuevo_mutex->bloqueados = list_create();

    list_add(lista_mutex, nuevo_mutex);

    pthread_mutex_unlock(&mutex_lista_mutex);
}


//MUTEX LOCK : Sirve para intentar tomar el mutex, o bloquear el proceso si ya está ocupado
bool mutex_lock(t_pcb* pcb, char* nombre) {
    if (pcb == NULL) {
        return true;
    }

    pthread_mutex_lock(&mutex_lista_mutex);

    t_mutex_kernel* mutex = buscar_mutex(nombre);

    if (mutex == NULL) {
        pthread_mutex_unlock(&mutex_lista_mutex);
        log_error(logger, "Mutex %s inexistente", nombre);
        return true;
    }

    // mutex libre
    if (mutex->duenio == NULL) {

        mutex->duenio = pcb;
        pthread_mutex_unlock(&mutex_lista_mutex);

        log_info(logger,
            "## (%d) Toma el Mutex %s",
            pcb->pid,
            nombre);

        return true;
    }

    if (mutex->duenio == pcb) {
        pthread_mutex_unlock(&mutex_lista_mutex);
        log_warning(logger, "## (%d) Intentó tomar nuevamente el Mutex %s", pcb->pid, nombre);
        return true;
    }

    // mutex ocupado -> BLOCK
    heredar_prioridad_si_corresponde(mutex, pcb);
    list_add(mutex->bloqueados, pcb);
    pthread_mutex_unlock(&mutex_lista_mutex);

    cambiar_estado(pcb, BLOCK);

    pthread_mutex_lock(&mutex_block);
    list_add(cola_block, pcb);
    pthread_mutex_unlock(&mutex_block);

    registrar_timer_block_mutex(pcb);

    log_info(logger,
        "## (%d) Bloqueado esperando Mutex %s",
        pcb->pid,
        nombre);

    return false;
}


//MUTEX UNLOCK: Sirve para liberar el mutex Y despertar al siguiente proceso esperando
void mutex_unlock(t_pcb* pcb, char* nombre) {
    pthread_mutex_lock(&mutex_lista_mutex);

    t_mutex_kernel* mutex = buscar_mutex(nombre);

    if (mutex == NULL) {
        pthread_mutex_unlock(&mutex_lista_mutex);
        return;
    }

    // validar dueño
    if (mutex->duenio != pcb) {
        pthread_mutex_unlock(&mutex_lista_mutex);

        log_warning(logger,
            "## (%d) Intentó liberar mutex sin ser dueño",
            pcb->pid);

        return;
    }

    log_info(logger,
        "## (%d) Libera el Mutex %s",
        pcb->pid,
        nombre);

    // nadie esperando
    if (list_is_empty(mutex->bloqueados)) {

        mutex->duenio = NULL;
        recalcular_prioridad_por_mutexes(pcb);
        pthread_mutex_unlock(&mutex_lista_mutex);
        return;
    }

    // desbloquear siguiente
    t_pcb* siguiente = list_remove(mutex->bloqueados, 0);

    mutex->duenio = siguiente;
    recalcular_prioridad_por_mutexes(pcb);
    aplicar_herencia_desde_bloqueados(mutex);
    pthread_mutex_unlock(&mutex_lista_mutex);

    log_info(logger,
        "## (%d) Toma el Mutex %s",
        siguiente->pid,
        nombre);

    eliminar_timer_block_mutex(siguiente);

    pthread_mutex_lock(&mutex_block);
    bool estaba_block = list_remove_element(cola_block, siguiente);
    pthread_mutex_unlock(&mutex_block);

    if (estaba_block) {
        agregar_a_ready(siguiente, BLOCK);
        return;
    }

    pthread_mutex_lock(&mutex_susp_block);
    bool estaba_susp_block = list_remove_element(cola_susp_block, siguiente);
    pthread_mutex_unlock(&mutex_susp_block);

    if (estaba_susp_block) {
        cambiar_estado(siguiente, SUSP_READY);

        pthread_mutex_lock(&mutex_susp_ready);
        list_add(cola_susp_ready, siguiente);
        pthread_mutex_unlock(&mutex_susp_ready);
        return;
    }

    agregar_a_ready(siguiente, siguiente->estado);
}

void liberar_mutexes_por_proceso(t_pcb* pcb) {
    if (pcb == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex_lista_mutex);

    for (int i = 0; i < list_size(lista_mutex); i++) {
        t_mutex_kernel* mutex = list_get(lista_mutex, i);

        // Si el proceso es dueño de este mutex, liberarlo y despertar al siguiente
        if (mutex->duenio == pcb) {
            log_info(logger, "## (%d) Libera el Mutex %s (por salida del proceso)", pcb->pid, mutex->nombre);

            // Si nadie está esperando, simplemente liberar el mutex
            if (list_is_empty(mutex->bloqueados)) {
                mutex->duenio = NULL;
                // No necesitamos recalcular prioridad ya que el proceso está saliendo
                continue;
            }

            // Desbloquear al siguiente proceso en espera
            t_pcb* siguiente = list_remove(mutex->bloqueados, 0);
            mutex->duenio = siguiente;

            log_info(logger, "## (%d) Toma el Mutex %s", siguiente->pid, mutex->nombre);

            // Aplicar herencia de prioridad desde los procesos que aún están bloqueados
            aplicar_herencia_desde_bloqueados(mutex);

            // Liberar el mutex de lista_mutex para evitar deadlock al modificar colas
            pthread_mutex_unlock(&mutex_lista_mutex);

            // Eliminar timer del proceso que despertamos
            eliminar_timer_block_mutex(siguiente);

            // Mover de BLOCK a READY
            pthread_mutex_lock(&mutex_block);
            bool estaba_block = list_remove_element(cola_block, siguiente);
            pthread_mutex_unlock(&mutex_block);

            if (estaba_block) {
                agregar_a_ready(siguiente, BLOCK);
            } else {
                // Verificar si estaba en SUSP_BLOCK
                pthread_mutex_lock(&mutex_susp_block);
                bool estaba_susp_block = list_remove_element(cola_susp_block, siguiente);
                pthread_mutex_unlock(&mutex_susp_block);

                if (estaba_susp_block) {
                    cambiar_estado(siguiente, SUSP_READY);
                    pthread_mutex_lock(&mutex_susp_ready);
                    list_add(cola_susp_ready, siguiente);
                    pthread_mutex_unlock(&mutex_susp_ready);
                } else {
                    agregar_a_ready(siguiente, siguiente->estado);
                }
            }

            // Re-lock para continuar iterando
            pthread_mutex_lock(&mutex_lista_mutex);
            // Decrementar i ya que la lista no cambió pero procesamos uno
            i--;
            continue;
        }

        // Si el proceso está bloqueado en este mutex, removerlo de la lista de bloqueados
        for (int j = 0; j < list_size(mutex->bloqueados); j++) {
            t_pcb* bloqueado = list_get(mutex->bloqueados, j);
            if (bloqueado == pcb) {
                list_remove(mutex->bloqueados, j);
                log_info(logger, "## (%d) Removido de la lista de bloqueados del Mutex %s (por salida del proceso)", pcb->pid, mutex->nombre);
                // No necesitamos recalcular prioridad ya que el proceso está saliendo
                break;
            }
        }
    }

    pthread_mutex_unlock(&mutex_lista_mutex);
}
