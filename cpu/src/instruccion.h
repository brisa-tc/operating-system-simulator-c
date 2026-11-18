#ifndef INSTRUCCION_H_
#define INSTRUCCION_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commons/string.h>
#include <commons/log.h>
#include "registros.h"

void ejecutar_NOOP(char** params);
void ejecutar_SET(char** params);
void ejecutar_MOV_IN(char** params);
void ejecutar_MOV_OUT(char** params);
void ejecutar_SUM(char** params);
void ejecutar_SUB(char** params);
void ejecutar_JNZ(char** params);
void ejecutar_COPY_MEM(char** params);
void ejecutar_MUTEX_CREATE(char** params);
void ejecutar_MUTEX_LOCK(char** params);
void ejecutar_MUTEX_UNLOCK(char** params);
void ejecutar_MEM_ALLOC(char** params);
void ejecutar_MEM_FREE(char** params);
void ejecutar_SLEEP(char** params);
void ejecutar_STDIN(char** params);
void ejecutar_STDOUT(char** params);
void ejecutar_INIT_PROC(char** params);
void ejecutar_EXIT(char** params);

void instruccion_no_implementada(char** params);

#endif