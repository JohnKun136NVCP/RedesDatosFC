#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/utsname.h>

#define BUFFER_SIZE 2048 // Tama~no del buffer para recibir datos

void encryptCaesar(char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' + shift) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' + shift) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}

int main(int argc, char *argv[]) {

	// Recibimos el puerto que debemos abrir
	if (argc != 2){
		perror("Uso: ./server <Puerto>\nPuertos disponibles: 49200, 49201, 49202");
		exit(1);
	}

	int port = atoi(argv[1]);

	// Restringimos los puertos a solo tres opciones validas
	if( port == 0 || (port != 49200 && port != 49201 && port != 49202)){
		perror("Uso: ./server <Puerto>\nEl puerto debe ser un numero de los siguientes: 49200, 49201, 49202");
		exit(1);
	}

	char buffer[BUFFER_SIZE];
	int server_sock, client_sock;
	struct sockaddr_in direccion, direccion_cliente;
	// Configuramos el socket
	socklen_t size_cliente;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = INADDR_ANY;
	direccion.sin_port = htons(port);

	if (bind(server_sock, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
		perror("Error al enlazar los sockets del servidor");
		close(server_sock);
		exit(1);
	}
	// El servidor queda en escucha...
	if (listen(server_sock, 3) < 0) {
		perror("Ocurrio un error en la espera de mensajes");
		close(server_sock);
		exit(1);
	}
	printf("[-][Server] Servidor escuchando puerto %d...\n", port);

	size_cliente = sizeof(direccion_cliente);

	// Queda en espera para recibir un mensaje, si el puerto no coincide continua escuchando,
	// cierra si hubo una conexion exitosa o un error de conexion
	while(1){

		client_sock = accept(server_sock, (struct sockaddr *)&direccion_cliente, &size_cliente);
		if (client_sock < 0){
			perror("[-] Error al aceptar la conexion");
			close(server_sock);
			exit(1);
		}
		printf("[+] Conexion establecida\n");

		int mensaje = recv(client_sock, buffer, sizeof(buffer), 0);
		if (mensaje < 0) {
			perror("[-] Error al recibir el mensaje");
			close(server_sock);
			close(client_sock);
			exit(1);
		}

		printf("[+] Mensaje recibido\n");
		int shift, puerto;
		char texto[BUFFER_SIZE];
		// Mensaje recibido: Archivo, puerto y desplazamiento
		sscanf(buffer, "%[^\n]\n%d\n%d", texto, &puerto, &shift);

		// Si el puerto recibido no es el de nuestro servidor rechazamos la conexion y avisamos al cliente
		// En otro caso aceptamos la conexion
		if (puerto != port) {
			printf("[-][Server] Peticion rechazada, puerto no correspondido(peticion para puerto %d)\n", puerto);
			send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
			close(client_sock);
		} else {
			send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
			encryptCaesar(texto, shift);
			sleep(1);
			// Enviamos el archivo de regreso, esta vez encriptado con Caesar
			if (send(client_sock, texto, strlen(texto), 0) < 0) {
				perror("[-] Error al enviar el archivo\n");
				close(server_sock);
				close(client_sock);
				exit(1);
			}

			printf("[+][Server] Archivo encriptado y enviado al cliente\n");
			// Terminamos el programa
			close(client_sock);
			close(server_sock);
			return 0;
		}
	}
}
