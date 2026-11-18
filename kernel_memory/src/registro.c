
#include "estructura.h"
#include "registro.h"


//setea todos los registros a cero
void iniciar_registros(t_registros *registro){
    registro->PC = 0;
    registro->AX = 0;
    registro->BX = 0;
    registro->CX = 0;
    registro->DX = 0;
    registro->EAX = 0;
    registro->EBX = 0;
    registro->ECX = 0;
    registro->EDX = 0;
    registro->DI = 0;
    registro->SI = 0;
}
