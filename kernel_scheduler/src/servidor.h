#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "types.h"

void* atender_cliente(void* arg);
t_io_kernel* buscar_io_por_socket(int socket);
bool solicitar_io(op_code tipo, t_pcb* pcb, uint32_t direccion, uint32_t tamanio, int tiempo);
void liberar_io(t_io_kernel* io);
void registrar_io(op_code tipo, int socket);
void manejar_desconexion_io(int socket);

#endif
