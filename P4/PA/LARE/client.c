#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 2048

// Funcion para procesar los mensajes recibidos
void recibeMensaje(int client_sock ,int bytes, char *buffer, size_t sizebuffer) {
	// Si no hubo errores en el mensaje
	if (bytes > 0) {
		buffer[bytes] = '\0';
		// Verificamos si el servidor nos dio acceso
		// Escribimos lo recibido y lo guardamos en un archivo
		FILE *fp = fopen("log.txt", "a");
		if (fp == NULL) {
			perror("[-] Error al abrir el archivo");
			close(client_sock);
			return;
		}
		while ((bytes = recv(client_sock, buffer, sizebuffer,0)) > 0) {
			fwrite(buffer, 1, bytes, fp);
		}
		printf("[+][Client] El log se guardo exitosamente'\n");
		fclose(fp);
	} else {
		printf("[-] Tiempo excedido\n"); // Si hubo un error de conexion o en el mensaje
	}
	return;
}

// Se agrega la peticion del cliente
void guardaLog(const char *status) {
	FILE *output = fopen("log.txt", "a");
	if (output == NULL) {
		perror("Error al intentar abrir el log de cliente");
        	return;
    	}

	// Obtenemos la informacion de tiempo
    	time_t now = time(NULL);
    	struct tm *t = localtime(&now);
    	char time_b[128];
    	strftime(time_b, sizeof(time_b), "%Y-%m-%d %H:%M:%S", t);

    	fprintf(output, "%s - %s\n", time_b, status);
    	printf("[+][Client] %s - %s\n", time_b, status);

    	fclose(output);
}


// Notificamos al cliente de todo lo que esta ocurriendo entre el y el servidor
void logCliente(int client_sock, int bytes, char *buffer, size_t sizebuffer) {
	// Si no hubo errores en el mensaje
	if (bytes > 0) {
		buffer[bytes] = '\0';
		printf("%s", buffer);
	} else {
		printf("[-] Tiempo excedido\n"); // Si hubo un error de conexion o en el mensaje
	}
}

int main(int argc, char *argv[]) {
	// Verificamos que recibimos 2 parametros
	if (argc != 4) {
		printf("Type: %s <ALIAS_IP> <PUERTO> <ARCHIVO> \n", argv[0]);
		return 1;
	}
	// Asignamos los diferentes parametros para su posterior uso
	int port = atoi(argv[2]);
	if (port == 0) {
		printf("Type: %s <ALIAS_IP> <PUERTO> <ARCHIVO> \nDebes usar el puerto 49200", argv[0]);
		return 1;
	}

	// Como recibimos un alias, debemos obtener la ip asociada al alias
	struct addrinfo hints, *res;
	char ip_str[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(argv[1], NULL, &hints, &res);
	if (status !=0) {
		perror("Error al obtener la ip del alias");
		return 1;
	}

	struct sockaddr_in *addr = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);

	// Limpiamos el nombre del archivo (ocurre errores en el envio)
	size_t len = strlen(argv[3]);
	while (len > 0 && (argv[3][len-1] == '\n' || argv[3][len-1] == '\r')) {
		argv[3][len-1] = '\0';
		len--;
	}

	// Preparamos el archivo de lectura
	FILE *input;
	char buffer[BUFFER_SIZE];
	char linea[100];
	input = fopen(argv[3], "r");

	if (input == NULL) {
		perror("Error al leer el archivo");
		return 1;
	}
	buffer[0] = '\0';
	// Leemos el archivo y lo concatenamos al buffer
	while (fgets(linea, sizeof(linea), input)) {
		strcat(buffer, linea);
	}
	fclose(input);
	// Para que el buffer sea una cadena valida
	buffer[BUFFER_SIZE-1] = '\0';

	// --- Preparamos el socket ---
	int client_sock;
	struct sockaddr_in serv_addr;
	char mensaje[BUFFER_SIZE];
	
	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock == -1) {
		perror("[-] Error al crear el socket");
		return 1;
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr = addr->sin_addr;
	freeaddrinfo(res);
	if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {
		perror("[-] Error al conectarse con el servidor");
		close(client_sock);
	}
	printf("[+] Conexion con los servidores\n");

	// Primer log recibido
	char *mensaje_log;
	
	int log;

	log = recv(client_sock, mensaje, sizeof(mensaje) -1, 0);

	if (log < 0) {
		mensaje_log = "ERROR: Fallo con el envio de estado del servidor";
		guardaLog(mensaje_log);
		close(client_sock);
		return 1;
	}
	logCliente(client_sock, log, mensaje, sizeof(mensaje));

	// Preparamos el mensaje y los enviamos al servidor
	
	// Primero enviamos el nombre del archivo
	send(client_sock, argv[3], sizeof(argv[3]), 0);
	send(client_sock, buffer, strlen(buffer), 0);	
	printf("[+][Client] El mensaje se ha enviado al servidor\n");

	// Segundo log recibido
	
	memset(mensaje, 0, sizeof(mensaje));

	log = recv(client_sock, mensaje, sizeof(mensaje) -1, 0);
	if (log < 0) {
		mensaje_log = "ERROR: fallo con el servidor al recibir el envio";
		guardaLog(mensaje_log);
		close(client_sock);
		return 1;
	}

	logCliente(client_sock, log, mensaje, sizeof(mensaje));
	

	// Tercer log recibido
	memset(mensaje, 0, sizeof(mensaje));

	log = recv(client_sock, mensaje, sizeof(mensaje) -1, 0);
	if (log < 0) {
		mensaje_log = "ERROR: No se pudo notificar al cliente";
		guardaLog(mensaje_log);
		close(client_sock);
		return 1;
	}
	logCliente(client_sock, log, mensaje, sizeof(mensaje));
	
	// Terminamos el programa
	guardaLog("OK");
	close(client_sock);
	return 0;
}
