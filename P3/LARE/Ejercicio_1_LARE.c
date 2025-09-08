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

// Crea sockets que seran nuestros distintos puertos en el servidor
int creaSockets(int *server_sock, struct sockaddr_in *configuracion, int puerto) {
	*server_sock = socket(AF_INET, SOCK_STREAM, 0);
	configuracion->sin_family = AF_INET;
	configuracion->sin_addr.s_addr = INADDR_ANY;
	configuracion->sin_port = htons(puerto);
	if (bind(*server_sock, (struct sockaddr *)configuracion, sizeof(*configuracion)) < 0) {
		perror("Error al enlazar los sockets del servidor");
		close(*server_sock);
		exit(1);	
	}
	if (listen(*server_sock, 3) <0) {
		perror("Ocurrio un error en la espera de mensajes");
		close(*server_sock);
		exit(1);
	}
}

// Procesa el mensaje recibido por el cliente, se espera un archivo y un desplazamiento
void procesaMensaje (int *cliente, int puerto) {	
	char buffer[BUFFER_SIZE];
	int mensaje = recv(*cliente, buffer, sizeof(buffer), 0);
	if (mensaje < 0) {
		printf("[-][PORT%d] Error al recibir el mensaje\n", puerto);
		close(*cliente);
		exit(1);
	}
	printf("[+][PORT:%d] Mensaje Recibido\n", puerto);

	int shift, puerto_msg;
	char texto[BUFFER_SIZE];

	sscanf(buffer, "%[^\n]\n%d\n%d", texto, &puerto_msg, &shift);

	send(*cliente, "ACCESS GRANTED\0", strlen("ACCESS GRANTED\0"), 0);
	sleep(1);
	encryptCaesar(texto,shift);

	if (send(*cliente, texto, strlen(texto), 0) < 0) {
		printf("[-][PORT:%d] Error al enviar el archivo\n", puerto);
		close(*cliente);
	}

	printf("[+][PORT:%d] Archivo encriptado y enviado al cliente\n", puerto);
	close(*cliente);
}

// Funcion principal
int main(int argc, char *argv[]) {

	int puertos[3] = {49200, 49201, 49202};
	int sockets[3];
	int cliente[3];
	char buffer[BUFFER_SIZE];
	struct sockaddr_in direccion[3];	
	
	// Configuramos el socket
	for (int i = 0; i < 3; i++) {
		creaSockets(&sockets[i], &direccion[i], puertos[i]);
	}
	printf("[-][Server] Servidor escuchando puertos...\n");

	// Haremos que los sockets funcionen de forma paralela para evitar bloqueos en el programa
	// Usaremos la funcion select(), por tanto debemos preparar los sockets junto
	// al descriptor para que esto sea posible
	
	// Descriptor
	fd_set sockets_connect;
	int max_desc = 0;
	// Queda en espera para recibir un mensaje
	while(1){

		// Establecemos cuales seran los sockets que revisaremos constantemente
		FD_ZERO (&sockets_connect);
		for (int i = 0; i < 3; i++){
			FD_SET (sockets[i], &sockets_connect);
			if (sockets[i] > max_desc) {
				max_desc = sockets[i];
			}
		}

		// Los seleccionamos y esperamos a que reciban una conexion
		int actividad;
		actividad = select(max_desc + 1, &sockets_connect, NULL, NULL, NULL);
		if (actividad < 0) {
			perror("Error en seleccion de sockets\n");
			exit(1);
		}

		// A partir de aqui, debemos revisar si algun socket recibio una peticion de conexion
		for (int i = 0; i < 3; i++) {
			if (FD_ISSET(sockets[i], &sockets_connect)) {
				struct sockaddr_in direccion_cliente;
				socklen_t size_cliente = sizeof(direccion_cliente);

				int cliente = accept(sockets[i], (struct sockaddr*)&direccion_cliente, &size_cliente);
				if (cliente < 0) {
					perror("[-] Error al establecer la conexion\n");
					exit(1);
				}
				printf("[+][PORT:%d] Conexion establecida\n", puertos[i]);
				procesaMensaje(&cliente, puertos[i]);
			}
		}

	}
}
