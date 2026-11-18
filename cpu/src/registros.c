#include "globals.h"
#include "registros.h"
#include "MMU.h"

void set_registro(char* nombre_reg, uint32_t valor) {
    
    // Registros de 8 bits
    if (string_equals_ignore_case(nombre_reg, "AX")) {
        registros->AX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_reg, "BX")) {
        registros->BX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_reg, "CX")) {
        registros->CX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_reg, "DX")) {
        registros->DX = (uint8_t)valor;
    } 
    
    // Registros de 32 bits
    else if (string_equals_ignore_case(nombre_reg, "EAX")) {
        registros->EAX = valor;
    } else if (string_equals_ignore_case(nombre_reg, "EBX")) {
        registros->EBX = valor;
    } else if (string_equals_ignore_case(nombre_reg, "ECX")) {
        registros->ECX = valor;
    } else if (string_equals_ignore_case(nombre_reg, "EDX")) {
        registros->EDX = valor;
    } else if (string_equals_ignore_case(nombre_reg, "SI")) {
        registros->SI = valor;
    } else if (string_equals_ignore_case(nombre_reg, "DI")) {
        registros->DI = valor;
    } else if (string_equals_ignore_case(nombre_reg, "PC")) {
        registros->PC = valor;
        fue_modificado_pc = true;
    } else {
        // Opcional: log_error(logger, "Registro no reconocido: %s", nombre_reg);
    }
}

void sumar_registros(char* reg_destino, char* reg_origen) {
    // 1. Obtener los valores actuales
    uint32_t valor_destino = get_registro(reg_destino);
    uint32_t valor_origen = get_registro(reg_origen);
    
    // 2. Calcular la suma
    uint32_t resultado = valor_destino + valor_origen;
    
    // 3. Guardar el resultado en el registro de destino
    // Reutilizamos la función set_registro que ya definimos antes
    set_registro(reg_destino, resultado);
}

void restar_registros(char* reg_destino, char* reg_origen) {
    // 1. Obtener los valores actuales
    uint32_t valor_destino = get_registro(reg_destino);
    uint32_t valor_origen = get_registro(reg_origen);
    
    // 2. Calcular la suma
    uint32_t resultado = valor_destino - valor_origen;
    
    // 3. Guardar el resultado en el registro de destino
    // Reutilizamos la función set_registro que ya definimos antes
    set_registro(reg_destino, resultado);
}

void jnz_registros(char* nombre_registro, uint32_t valor) {
    uint32_t valor_registro = get_registro(nombre_registro);

    if(valor_registro != 0){
        set_registro("PC", valor);
    }
}

static int tamanio_registro(char* nombre_registro) {
    if (string_equals_ignore_case(nombre_registro, "AX") ||
        string_equals_ignore_case(nombre_registro, "BX") ||
        string_equals_ignore_case(nombre_registro, "CX") ||
        string_equals_ignore_case(nombre_registro, "DX"))
        return sizeof(uint8_t);

    return sizeof(uint32_t);
}

t_resultado_memoria_cpu move_in_registros(char* reg_destino){
    uint32_t direccion_logica = get_registro("SI");
    int tamanio = tamanio_registro(reg_destino);
    void* datos = NULL;
    t_resultado_memoria_cpu resultado = leer_memoria(direccion_logica, tamanio, &datos);
    if (resultado != MEM_CPU_OK)
        return resultado;

    uint32_t valor = 0;
    memcpy(&valor, datos, tamanio);
    set_registro(reg_destino, valor);
    free(datos);
    return MEM_CPU_OK;
}

t_resultado_memoria_cpu move_out_registros(char* registro){
    uint32_t valor_registro = get_registro(registro);
    uint32_t direccion_logica = get_registro("DI");
    return escribir_memoria(direccion_logica, &valor_registro, tamanio_registro(registro));
}

t_resultado_memoria_cpu copy_mem_registros(char* registro){
    uint32_t cant_bytes = get_registro(registro);
    uint32_t dir_origen = get_registro("SI");
    uint32_t dir_destino = get_registro("DI");

    void* datos = NULL;
    t_resultado_memoria_cpu resultado = leer_memoria(dir_origen, (int)cant_bytes, &datos);
    if (resultado != MEM_CPU_OK)
        return resultado;

    resultado = escribir_memoria(dir_destino, datos, (int)cant_bytes);
    free(datos);
    return resultado;
}

void avanzar_pc(){
    registros->PC++;
}

uint32_t get_registro(char* nombre_reg) {
    if (string_equals_ignore_case(nombre_reg, "AX")) return (uint32_t)registros->AX;
    if (string_equals_ignore_case(nombre_reg, "BX")) return (uint32_t)registros->BX;
    if (string_equals_ignore_case(nombre_reg, "CX")) return (uint32_t)registros->CX;
    if (string_equals_ignore_case(nombre_reg, "DX")) return (uint32_t)registros->DX;
    
    if (string_equals_ignore_case(nombre_reg, "EAX")) return registros->EAX;
    if (string_equals_ignore_case(nombre_reg, "EBX")) return registros->EBX;
    if (string_equals_ignore_case(nombre_reg, "ECX")) return registros->ECX;
    if (string_equals_ignore_case(nombre_reg, "EDX")) return registros->EDX;
    
    if (string_equals_ignore_case(nombre_reg, "SI")) return registros->SI;
    if (string_equals_ignore_case(nombre_reg, "DI")) return registros->DI;
    if (string_equals_ignore_case(nombre_reg, "PC")) return registros->PC;

    return 0; // O manejar error
}
