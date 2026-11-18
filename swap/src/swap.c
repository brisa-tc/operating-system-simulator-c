#include "swap.h"

bool bloque_valido(int bloque, int block_count) {
    return bloque >= 0 && bloque < block_count;
}

bool direccion_swap_valida(int direccion, int tamanio, int swap_file_size) {
    return direccion >= 0 && direccion + tamanio <= swap_file_size;
}
