#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 2048

// Funcion para procesar los mensajes recibidos
void recibeMensaje(int client_sock ,int bytes, char *buffer, size_t sizebuffer, int puerto) {
	// Si no hubo errores en el mensaje
	if (bytes > 0) {
		buffer[bytes] = '\0';
		// Verificamos si el servidor nos dio acceso
		if (strstr(buffer, "ACCESS GRANTED") != NULL) {
			printf("[+][Client] Conexion establecida con el puerto %d\n", puerto);
			// Escribimos lo recibido y lo guardamos en un archivo
			FILE *fp = fopen("encrypted.txt", "w");
			if (fp == NULL) {
				perror("[-] Error al abrir el archivo");
				close(client_sock);
				return;
			}
			while ((bytes = recv(client_sock, buffer, sizebuffer,0)) > 0) {
				fwrite(buffer, 1, bytes, fp);
			}
			printf("[+][Client] El archivo se guardo como 'encrypted.txt'\n");
			fclose(fp);
		} else {
			printf("[-] Acceso denegado al servidor %d...\n", puerto);
		}
	} else {
		printf("[-] Tiempo excedido\n"); // Si hubo un error de conexion o en el mensaje
	}
}

int main(int argc, char *argv[]) {
	// Verificamos que recibimos los 5 parametros
	if (argc != 5) {
		printf("Type: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Archivo>\n", argv[0]);
		return 1;
	}
	// Asignamos los diferentes parametros para su posterior uso
	char *server_ip = argv[1];
	int port = atoi(argv[2]);
	// Parseamos a un int...
	if (port == 0 || (port != 49200 && port != 49201 && port != 49202)) {
		printf("Type: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Archivo>\nEl puerto debe ser alguno de estos: 49200, 49201, 49202\n", argv[0]);
		return 1;
	}
	int shift = atoi(argv[3]);
	if (shift == 0) {
		printf("Type: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Archivo>\nEl desplazamiento debe ser un numero", argv[0]);
		return 1;
	}

	// Preparamos el archivo de lectura
	FILE *input;
	char buffer[BUFFER_SIZE];
	char linea[100];
	input = fopen(argv[4], "r");

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

	// --- Preparamos los enchufes para cada servidor ---
	int client_sock1, client_sock2, client_sock3;
	struct sockaddr_in serv_addr;
	char mensaje[BUFFER_SIZE];
	
	client_sock1 = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock1 == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(49200);
	serv_addr.sin_addr.s_addr = inet_addr(server_ip);
	if (connect(client_sock1, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {
		perror("[-] Error to connect to 49200");
		close(client_sock1);
	}

	client_sock2 = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock2 == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}
	serv_addr.sin_port = htons(49201);
	if (connect(client_sock2, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {
		perror("[-] Error to connect to 49201");
		close(client_sock2);
	}

	client_sock3 = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock3 == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}
	serv_addr.sin_port = htons(49202);
	if (connect(client_sock3, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {
		perror("[-] Error to connect to 49202");
		close(client_sock3);
	}
	printf("[+] Conexion con los servidores\n");

	// Preparamos el mensaje y los enviamos a los diferentes servidores
	snprintf(mensaje, sizeof(mensaje), "%s\n%d\n%d", buffer, port, shift);
	send(client_sock1, mensaje, strlen(mensaje), 0);
	send(client_sock2, mensaje, strlen(mensaje), 0);
	send(client_sock3, mensaje, strlen(mensaje), 0);	
	printf("[+][Client] El mensaje se ha enviado a los servidores\n");

	// Recibimos los mensajes de los distintos servidores y procesamos dichos mensajes
	char buffer1[BUFFER_SIZE];
	char buffer2[BUFFER_SIZE];
	char buffer3[BUFFER_SIZE];
	int bytes1 = recv(client_sock1, buffer1, sizeof(buffer1) - 1, 0);
	recibeMensaje(client_sock1, bytes1, buffer1, sizeof(buffer1), 49200);
	int bytes2 = recv(client_sock2, buffer2, sizeof(buffer2) - 1, 0);
	recibeMensaje(client_sock2, bytes2, buffer2, sizeof(buffer2), 49201);
	int bytes3 = recv(client_sock3, buffer3, sizeof(buffer3) - 1, 0);
	recibeMensaje(client_sock3, bytes3, buffer3, sizeof(buffer3), 49202);

	// Terminamos el programa
	close(client_sock1);
	close(client_sock2);
	close(client_sock3);
	return 0;
}
