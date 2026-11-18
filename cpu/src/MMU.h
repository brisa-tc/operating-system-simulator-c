#ifndef MMU_H_
#define MMU_H_
#include <stdint.h>

typedef enum {
    MEM_CPU_OK,
    MEM_CPU_SEG_FAULT,
    MEM_CPU_ERROR
} t_resultado_memoria_cpu;

t_resultado_memoria_cpu leer_memoria(uint32_t dir_logica, int tamanio, void** contenido);
t_resultado_memoria_cpu escribir_memoria(uint32_t dir_logica, const void* contenido, int tamanio);
t_resultado_memoria_cpu traducir_rango_logico(uint32_t dir_logica, int tamanio, uint32_t* dir_fisica);
bool obtener_info_ms_por_direccion_fisica(uint32_t dir_fisica, int* socket, uint32_t* base, int* tamanio);

#endif
