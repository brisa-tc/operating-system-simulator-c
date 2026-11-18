#ifndef RESGISTROS_H_
#define RESGISTROS_H_

#include <inttypes.h>
#include <commons/string.h>
#include <stdint.h>
#include "utils/contexto_utils.h"
#include "MMU.h"


void set_registro(char* nombre_reg, uint32_t valor);
void sumar_registros(char* reg_destino, char* reg_origen);
void restar_registros(char* reg_destino, char* reg_origen);
void jnz_registros(char* nombre_registro, uint32_t valor);

t_resultado_memoria_cpu move_in_registros(char* reg_destino);
t_resultado_memoria_cpu move_out_registros(char* registro);
t_resultado_memoria_cpu copy_mem_registros(char* registro);


void avanzar_pc();
uint32_t get_registro(char* nombre_reg);

#endif
