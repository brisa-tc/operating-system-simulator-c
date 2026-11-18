#include "init.h"
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

t_log* logger;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [Archivo Config] [Path Proceso Inicial]\n", argv[0]);
        return EXIT_FAILURE;
    }

    iniciar_modulo(argv[1], argv[2]);

    return 0;
}