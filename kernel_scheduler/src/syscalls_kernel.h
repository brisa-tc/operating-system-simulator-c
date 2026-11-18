#ifndef SYSCALLS_KERNEL_H
#define SYSCALLS_KERNEL_H

#include <utils/syscall.h>
#include "types.h"

void procesar_syscall_kernel(int socket_cpu, t_syscall* syscall);
void completar_io_de_proceso(t_pcb* pcb);
void finalizar_proceso_bloqueado_por_error(t_pcb* pcb, const char* motivo);

#endif
