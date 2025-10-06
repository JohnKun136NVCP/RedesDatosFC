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
#define PORT 49200

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
int creaSockets(int *server_sock, struct sockaddr_in *configuracion, char *alias, int desplazamiento) {
	// Como recibimos un alias, debemos obtener la ip asociado al alias
	//
	struct addrinfo hints, *res;
	char ip_str[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(alias, NULL, &hints, &res);
	if (status !=0) {
		perror("Error al obtener la ip del alias");
		exit(1);
	}

	struct sockaddr_in *addr = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);	

	// Configuramos el socket

	*server_sock = socket(AF_INET, SOCK_STREAM, 0);

	int opt = 1;
	setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	configuracion->sin_family = AF_INET;
	configuracion->sin_addr = addr->sin_addr;
	configuracion->sin_port = htons(PORT);
	freeaddrinfo(res);
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

char* recuperaAlias(char* ip) {
	if (strcmp(ip, "192.168.1.101") == 0){
		return "s01";
	} else if (strcmp(ip, "192.168.1.102") == 0){
		return "s02";
	} else if (strcmp(ip, "192.168.1.103") == 0){
		return "s03";
	} else if (strcmp(ip, "192.168.1.104") == 0){
		return "s04";
	} else {
		return NULL;
	}
}

// Procesa el mensaje recibido por el cliente, se espera un archivo y un desplazamiento
void interaccionCliente_Servidor (int *client_sock, char *alias) {
	char buffer[BUFFER_SIZE];
	send(*client_sock, "[Server:] En espera de mensajes...\n", strlen("[Server:] En espera de mensajes...\n"), 0);

	char filename[100];
	// Recibimos el nombre del archivo
	int name = recv(*client_sock, filename, sizeof(filename),0);
	if (name < 0) {
		perror("[-] Error al recibir el nombre del archivo");
		close(*client_sock);
		exit(1);
	}
	// Recibimos el contenido del archivo
	int mensaje = recv(*client_sock, buffer, sizeof(buffer), 0);
	if (mensaje < 0) {
		perror("[-] Error al recibir el mensaje");
		close(*client_sock);
		exit(1);
	}
	send(*client_sock, "[Server:]Recibiendo mensaje\n", strlen("[Server:]Recibiendo mensaje\n"), 0);
	printf("[+][IP:%s] Mensaje recibido\n", alias);
	char texto[BUFFER_SIZE];
	// Mensaje recibido: Escribimos en el directorio correspondiente:
	char *home = getenv("HOME");
	if (home == NULL) {
		perror("[-] Error con PATH, cerrando servidor");
		exit(1);
	}
	// Checamos el mensaje recibido
	char ruta[1024];
	// Creamos la ruta
	snprintf(ruta, sizeof(ruta), "%s/%s/%s", home,alias, filename);
	// Guardamos el contenido en un archivo
	FILE *output;
	output = fopen(ruta, "w");
	printf("[+][IP:%s]Escribiendo en el archivo\n", alias);

	if (output) {
		fprintf(output, "%s", buffer);
		fclose(output);
	} else {
		printf(ruta);
		perror("Error al escribir en el archivo\n");
		exit(1);
	}
	printf("[+][IP:%s] Archivo guardado\n",alias);
	sleep(1);
	// Enviamos una notificacion al cliente que el mensaje fue recibido
	if (send(*client_sock, "[Server:] Archivo guardado\n", strlen("[Server:] Archivo guardado\n"), 0) < 0) {
		perror("[-] Error al enviar el archivo\n");
		close(*client_sock);
		exit(1);
	}
}

// Funcion principal
int main(int argc, char *argv[]) {

	if (argc < 2) {
		perror("Uso: ./server <ALIAS>\nDebe de haber al menos un alias, con un maximo de 4 alias entre s01, s02, s03 y s04");
		exit(1);
	}
	int sockets[argc-1];
	int cliente[argc-1];
	struct sockaddr_in direccion[argc-1];	
	
	// Configuramos el socket
	for (int i = 1; i < argc; i++) {
		creaSockets(&sockets[i-1], &direccion[i-1], argv[i], i-1); // Puertos dinamicos
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
		for (int i = 0; i < argc-1; i++){
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
		for (int i = 0; i < argc-1; i++) {
			if (FD_ISSET(sockets[i], &sockets_connect)) {
				struct sockaddr_in direccion_cliente;
				socklen_t size_cliente = sizeof(direccion_cliente);

				int cliente = accept(sockets[i], (struct sockaddr*)&direccion_cliente, &size_cliente);
				if (cliente < 0) {
					perror("[-] Error al establecer la conexion\n");
					exit(1);
				}
				char server_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &direccion[i].sin_addr, server_ip, sizeof(server_ip));
				printf("[+]Conexion establecida\n");
				char *alias = recuperaAlias(server_ip);
				interaccionCliente_Servidor(&cliente, alias);
			}
		}

	}
}
