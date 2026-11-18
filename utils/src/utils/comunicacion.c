#include "comunicacion.h"

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char* ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int error = getaddrinfo(ip, puerto, &hints, &server_info);
	if (error != 0) {
		log_error(logger, "Error al resolver %s:%s - %s", ip, puerto, gai_strerror(error));
		return -1;
	}

	int intento = 0;
	while (1) {
		intento++;

		int socket_cliente = socket(server_info->ai_family,
									server_info->ai_socktype,
									server_info->ai_protocol);

		if (socket_cliente == -1) {
			log_error(logger, "Error al crear socket (intento %d)", intento);
			sleep(CONEXION_RETRY_INTERVAL);
			continue;
		}

		if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == 0) {
			freeaddrinfo(server_info);
			return socket_cliente;
		}

		close(socket_cliente);
		log_warning(logger, "No se pudo conectar a %s:%s (intento %d), reintentando en %ds...",
					ip, puerto, intento, CONEXION_RETRY_INTERVAL);
		sleep(CONEXION_RETRY_INTERVAL);
	}
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

int iniciar_servidor(char* puerto)
{
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family,
							 servinfo->ai_socktype,
							 servinfo->ai_protocol);

	setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	// Asociamos el socket a un puerto
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	// Escuchamos las conexiones entrantes
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{	
	// Aceptamos un nuevo cliente
	int socket_cliente;
	socket_cliente = accept(socket_servidor, NULL, NULL);
	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;
	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_debug(logger, "%s", buffer);
	free(buffer);
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void iniciar_servidor_multiplexador(char *puerto, int *server_fd, void* (*atender_cliente)(void*))
{
    if (puerto == NULL || server_fd == NULL || atender_cliente == NULL) {
        log_error(logger, "Parámetros inválidos");
        return;
    }

    *server_fd = iniciar_servidor(puerto);
    if (*server_fd == -1) {
        log_error(logger, "Fallo al iniciar el servidor");
        return;
    }

    log_info(logger, "Servidor listo para recibir conexiones");
    while (1)
    {
        pthread_t thread;
        int *fd_conexion_ptr = malloc(sizeof(int));
        if (fd_conexion_ptr == NULL) {
            log_error(logger, "No se pudo reservar memoria para el cliente");
            continue;
        }

        *fd_conexion_ptr = esperar_cliente(*server_fd);
		log_debug(logger, "socket cliente: %d", *fd_conexion_ptr);
        if (*fd_conexion_ptr == -1)
        {
            log_error(logger, "Error al aceptar cliente");
            free(fd_conexion_ptr);
            continue;
        }
        else
        {
            log_debug(logger, "Conexion recibida");
        }
        pthread_create(&thread, NULL, atender_cliente, (void *)fd_conexion_ptr);
        pthread_detach(thread);
    }
}


void enviar_codigo(op_code codigo, int socket) {
    send(socket, &codigo, sizeof(int), 0);
}

char* obtener_ip_cliente(int cliente_fd){
	struct sockaddr_in cliente_addr;
	socklen_t addr_len = sizeof(cliente_addr);

	// 'cliente_fd' es el descriptor de la conexión aceptada
	getpeername(cliente_fd, (struct sockaddr *)&cliente_addr, &addr_len);

	char *ip_str = malloc(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &cliente_addr.sin_addr, ip_str, INET_ADDRSTRLEN);

	log_debug(logger, "La IP del cliente es: %s\n", ip_str);
	return ip_str;
}
