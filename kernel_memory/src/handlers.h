#ifndef HANDLERS_H
#define HANDLERS_H

#include "estructura.h"
#include "procesos.h"
#include "segmentos.h"
#include <utils/comunicacion.h>
#include <utils/identificadores.h>



void atender_create_process(int socket_cliente);
void atender_finish_process(int socket_cliente);
void atender_suspend_process(int socket_cliente);
void atender_desuspend_process(int socket_cliente);
void atender_create_segment(int socket_cliente);
void atender_delete_segment(int socket_cliente);
void atender_mem_read(int socket_cliente);
void atender_mem_write(int socket_cliente);

#endif
