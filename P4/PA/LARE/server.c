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
#include <netdb.h>


#define BUFFER_SIZE 2048 // Tama~no del buffer para recibir datos
#define PORT 49200 // Siempre abriremos este puerto
		   
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

	// Recibimos el alias a iniciar
	if (argc != 2){
		perror("Uso: ./server <Alias>\nAlias disponibles: s01, s02");
		exit(1);
	}

	char buffer[BUFFER_SIZE];
	int server_sock, client_sock;
	struct sockaddr_in direccion, direccion_cliente;

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

	// Configuramos el socket
	socklen_t size_cliente;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	direccion.sin_family = AF_INET;
	direccion.sin_addr = addr->sin_addr;
	direccion.sin_port = htons(PORT);
	freeaddrinfo(res);

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
	printf("[-][Server:%s] Servidor escuchando puerto %d...\n", ip_str, PORT);

	size_cliente = sizeof(direccion_cliente);

	// Queda en espera para recibir un mensaje, si el puerto no coincide continua escuchando,
	// 
	while(1){

		client_sock = accept(server_sock, (struct sockaddr *)&direccion_cliente, &size_cliente);
		if (client_sock < 0){
			perror("[-] Error al aceptar la conexion");
			close(server_sock);
			exit(1);
		}
		printf("[+] Conexion establecida\n");
		send(client_sock, "[Server:] En espera de mensajes...\n", strlen("[Server:] En espera de mensajes...\n"), 0);

		char filename[100];
		// Recibimos el nombre del archivo
		int name = recv(client_sock, filename, sizeof(filename),0);
		if (name < 0) {
			perror("[-] Error al recibir el nombre del archivo");
			close(server_sock);
			close(client_sock);
			exit(1);
		}
		// Recibimos el contenido del archivo
		int mensaje = recv(client_sock, buffer, sizeof(buffer), 0);
		if (mensaje < 0) {
			perror("[-] Error al recibir el mensaje");
			close(server_sock);
			close(client_sock);
			exit(1);
		}
		send(client_sock, "[Server:]Recibiendo mensaje\n", strlen("[Server:]Recibiendo mensaje\n"), 0);
		printf("[+] Mensaje recibido\n");
		char texto[BUFFER_SIZE];
		// Mensaje recibido: Escribimos en el directorio correspondiente:
		char *home = getenv("HOME");
		if (home == NULL) {
			perror("[-] Error con PATH, cerrando servidor");
			return -1;
		}
		// Checamos el mensaje recibido

		char ruta[1024];
		// Creamos la ruta
		snprintf(ruta, sizeof(ruta), "%s/%s/%s", home,argv[1], filename);

		// Guardamos el contenido en un archivo
		FILE *output;
		output = fopen(ruta, "w");
		printf("[+]Escribiendo en el archivo\n");

		if (output) {
			fprintf(output, "%s", buffer);
			fclose(output);
		} else {
			printf(ruta);
			perror("Error al escribir en el archivo\n");
			return 1;
		}
		printf("[+] Archivo guardado\n");

		sleep(1);
		// Enviamos el archivo de regreso, esta vez encriptado con Caesar
		if (send(client_sock, "[Server:] Archivo guardado\n", strlen("[Server:] Archivo guardado\n"), 0) < 0) {
			perror("[-] Error al enviar el archivo\n");
			close(client_sock);
			exit(1);
		}

	}
}
