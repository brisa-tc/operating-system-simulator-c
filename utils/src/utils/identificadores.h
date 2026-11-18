#ifndef IDENTIFICADORES_H
#define IDENTIFICADORES_H

#include <stdlib.h>
#include <stdio.h>
#include "comunicacion.h"

/**
* @brief Envia un identificador o codigo de operacion al servidor 
* @param identificador MENSAJE_CPU, MENSAJE_SCHEDULER, MENSAJE_MEMORY_STICK, MENSAJE_SWAP, MENSAJE_IO,
* @param socket_servidor es el socket del destinatario
* @return No devuelve nada
*/
void enviar_identificador(int identificador, int socket_servidor);

#endif