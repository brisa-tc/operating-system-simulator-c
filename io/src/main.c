#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <utils/comunicacion.h>
#include <utils/io_utils.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <unistd.h>

t_log *logger;
int pid = 0;

int main(int argc, char* argv[]) {

    //Inicio de modulo
    t_config *config = config_create(argv[1]);
    char *tipo = string_duplicate(argv[2]);
    char *ip_kernel_scheduler = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char *puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    char* log_level = config_get_string_value(config, "LOG_LEVEL");

    //acceder al logger correspondiente al tipo de IO
    char *log_filename = string_from_format("log_io_%s.log", tipo);
    char *log_procname = string_from_format("io_%s", tipo);
    logger = log_create(log_filename, log_procname, true, log_level_from_string(log_level));
    free(log_filename);
    free(log_procname);

    //obtener el tipo enum
    op_code tipo_io;
    if (string_equals_ignore_case(tipo, "STDIN"))
    {
        tipo_io = IO_STDIN;
    }else if (string_equals_ignore_case(tipo, "STDOUT"))
    {
        tipo_io = IO_STDOUT;
    }
    else if (string_equals_ignore_case(tipo, "SLEEP"))
    {
        tipo_io = IO_SLEEP;
    }else{
        log_error(logger, "Tipo de IO no reconocido");
        config_destroy(config);
        log_destroy(logger);
        free(tipo);
        return EXIT_FAILURE;
    }
    
    //logear datos del config
    log_debug(logger, "LOG LEVEL: %s",log_level);
    log_debug(logger, "%s", tipo);

    log_debug(logger, "IP KERNEL SCHEDULER: %s", ip_kernel_scheduler);
    log_debug(logger, "PUERTO KERNEL SCHEDULER: %s", puerto_kernel_scheduler);


    //conectarse a kernel scheduler
    int socket_kernel_scheduler = crear_conexion(ip_kernel_scheduler, puerto_kernel_scheduler);
    if (socket_kernel_scheduler == -1)
    {
        log_error(logger, "No se pudo conectar al kernel scheduler");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    enviar_codigo(MENSAJE_IO, socket_kernel_scheduler);
    enviar_codigo(tipo_io, socket_kernel_scheduler);
    log_info(logger, "## Conectado a Kernel Scheduler");

    while (1)
    {
    //esperar conexion
    int codigo_operacion = recibir_operacion(socket_kernel_scheduler);

    if (codigo_operacion == -1) {
        log_warning(logger, "Kernel Scheduler desconectado");
        break;
    }

    if (codigo_operacion != tipo_io) {
        log_warning(logger, "Operacion no esperada: %d", codigo_operacion);
        continue;
    }

    //tratar conexion
    switch (codigo_operacion)
    {
    case IO_STDIN:

    //recibe el PID la cantidad de caracteres a leer
    pid = 0;
    int cant_leer = 0;
    recibir_pedido_io_stdin(socket_kernel_scheduler, &pid, &cant_leer);

    log_info(logger, "## PID: %d - Inicio de IO", pid);
    log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, cant_leer);

    //leemos la entrada del usuario
    char *leido;
	leido = readline("> ");

    if(string_length(leido) > cant_leer){
        char *nuevo_leido = string_substring_until(leido, cant_leer);
        free(leido);
        leido = string_duplicate(nuevo_leido);
    }else{
        //rellenar el string 
        char *ceros = string_repeat('\0', cant_leer - string_length(leido));
        string_append(&leido, ceros);
        free(ceros);
    }

    log_debug(logger, "%s", leido);

    log_info(logger, "## PID: %d - Fin de IO", pid);
    //enviar los caracteres a kernel scheduler
    enviar_respuesta_io_stdin(leido, socket_kernel_scheduler);

    //limpiar
    free(leido);

        break;
    case IO_STDOUT:

    //simular obtener la cadena de caracteres
    pid = 0;
    char *contenido = string_new();
    recibir_pedido_io_stdout(socket_kernel_scheduler, &pid, &contenido);

    log_info(logger, "## PID: %d - Inicio de IO", pid);
    log_info(logger, "## PID: %d - %s", pid, contenido);
    log_info(logger, "## PID: %d - Fin de IO", pid);

    //informar a kernel scheduler
    enviar_respuesta_io_generica(IO_STDOUT, true, socket_kernel_scheduler);

    free(contenido);
        break;
    case IO_SLEEP:

    //obtener el tiempo
    pid = 0;
    int tiempo = 0;
    recibir_pedido_io_sleep(socket_kernel_scheduler, &pid, &tiempo);

    log_info(logger, "## PID: %d - Inicio de IO", pid);
    log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, tiempo);

    usleep(tiempo * 1000);

    log_info(logger, "## PID: %d - Fin de IO", pid);

    //informar a kernel scheduler
    enviar_respuesta_io_generica(IO_SLEEP, true, socket_kernel_scheduler);

        break;
    default:
        break;
    }

    }
    //liberar memoria
    config_destroy(config);
    log_destroy(logger);
    free(tipo);
  
    return 0;
}
