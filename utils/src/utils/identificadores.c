#include "identificadores.h"

void enviar_identificador(int identificador, int socket_servidor){
    int cod_op = identificador;
    send(socket_servidor, &cod_op, sizeof(int), 0);
}