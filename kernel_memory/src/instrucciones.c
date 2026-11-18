#include "instrucciones.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* obtener_instruccion(char* path, int pc)
{
    FILE* archivo = fopen(path, "r");

    if(archivo == NULL)
        return NULL;

    char* linea = NULL;
    size_t len = 0;

    int linea_actual = 0;

    while(getline(&linea, &len, archivo) != -1)
    {
        // sacar salto de linea
        linea[strcspn(linea, "\n")] = '\0';

        if(linea_actual == pc)
        {
            fclose(archivo);
            return linea;
        }

        linea_actual++;
    }

    fclose(archivo);

    free(linea);

    return NULL;
}