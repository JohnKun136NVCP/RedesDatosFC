#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>	 // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>	 // Funciones generales (exit, malloc, etc.)
#include <string.h>	 // Manejo de cadenas (strlen, strcpy, etc.)
#include <ctype.h>
#include <stdbool.h>	 // Tipo booleano (true/false)
#include <sys/select.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

/* Función de cifrado César a la inversa */
void decryptCaesar(char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}


/* Función de cifrado César
   Nos movemos del lado opuesto ahora*/
void cryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
            char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}


int crear_socket(int *server_sock, int PORT) {
	// Crea un socket.
	*server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (*server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}

	// Permitir múltiples puertos en una IP
	int reuse = 1; // Valor para activar la opción
	if (setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		perror("No se pudo establecer la conexión a los puertos");
		return 1;
	}

	// Configura la dirección del servidor (IPv4, puerto, cualquier IP local).
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Asocia el socket al puerto.
	if (bind(*server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
		printf("%d", PORT);
		perror("[-] Error binding");
		close(*server_sock);
		return 1;
	}
	// Poner el socket en modo escucha.
	if (listen(*server_sock, 4) < 0) {
		perror("[-] Error on listen");
		close(*server_sock);
		return 1;
	}
	printf("[+][Server %d] Server listening port %d...\n", PORT, PORT);

	return 0;
}


int manejar_peticion(int server_sock, int PORT) {
	int client_sock;
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0};
	char text[BUFFER_SIZE];
	int shift, requested_port;

	// Espera y acepta una conexión entrante.
	addr_size = sizeof(client_addr);
	client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
	if (client_sock < 0) {
		perror("[-] Error on accept");
		return 1;
	}
	// Recibe el mensaje.
	// printf("[+][Server] Client connected\n");
	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		printf("[-][Server %d] Missed information\n", PORT);
		close(client_sock);
		return 1;
	}
	// Extrae desplazamiento
	buffer[bytes] = '\0';
	sscanf(buffer, "%d %d", &shift, &requested_port);
	// Encriptar texto dado	
    strncpy(text, buffer + 8, sizeof(buffer)-8);
    cryptCaesar(text, shift);
    printf("[+][Server %d] File recived and encrypted: \n%s\n", PORT, text);
	// Confirmar petición
    send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
    sleep(1); // Peque~na pausa para evitar colision de datos
	// Libera los recursos de red.
	close(client_sock);
	return 0;
}


int manejar_conexiones(int *server_sock, int *ports) {
	// Conjunto de sockets
	fd_set current_sockets, ready_sockets;
	int port_size = sizeof(ports) / sizeof(ports[0]);
	FD_ZERO(&current_sockets);
	for(int i=0; i<=port_size; i++) {
		FD_SET(server_sock[i], &current_sockets);
	}
	// Revisar los puertos constantemente
	while(true) {
		ready_sockets = current_sockets; // select es destructiva
		// Ver conexiones listas
		if(select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
			perror("[-] Error on select");
			return 1;
		}
		// Manejar peticiones
		for(int i=0; i<=port_size; i++) {
			if(FD_ISSET(server_sock[i], &ready_sockets)) {
				manejar_peticion(server_sock[i], ports[i]);
			}
		}
	}
	return 0;
}


// Función main
int main(int argc, char *argv[]) {
    // Verificar parámetros
    if (argc != 1) {
        printf("Type: %s \n", argv[0]);
        return 1;
    }

	// Puertos a usar
	int ports[] = {49200, 49201, 49202};
	int port_size = sizeof(ports) / sizeof(ports[0]);
	int server_sock[port_size]; 
	
	// Crear sockets para escuchar en puertos
	for(int i=0; i<port_size; i++) {
		if(crear_socket(&server_sock[i], ports[i]) != 0) {
			return 1;
		}
	}

	// Manejar peticiones del cliente
	manejar_conexiones(server_sock, ports);

	// Cerrar sockets
	for(int i=0; i<port_size; i++) {
		close(server_sock[i]);
	}	
	return 0;
}