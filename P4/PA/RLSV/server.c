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
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.1.76"
#define S01_IP "192.168.1.101"
#define S02_IP "192.168.1.102"
#define SERVER_PORT 49200

char *server_ip = SERVER_IP;

char* get_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    static char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return buf;
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
	server_addr.sin_addr.s_addr = inet_addr(server_ip);

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


int manejar_peticion(int *server_sock, int PORT) {
	int client_sock;
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0}, state[BUFFER_SIZE], message[BUFFER_SIZE] = {0};

    snprintf(state, sizeof(state), "[%d][%s] Server listening port %d...\n", PORT, get_time(), PORT);
	strcat(message, state); 
	// Espera y acepta una conexión entrante.
    snprintf(state, sizeof(state), "[%d][%s] Accepting conection...\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 
	addr_size = sizeof(client_addr);
	client_sock = accept(*server_sock, (struct sockaddr *)&client_addr,&addr_size);
	if (client_sock < 0) {
		perror("[-] Error on accept");
		return 1;
	}
    snprintf(state, sizeof(state), "[%d][%s] Client connected.\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 
    
	// Recibe el mensaje.
    snprintf(state, sizeof(state), "[%d][%s] Receiving the message...\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 
	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		printf("[-][Server %d] Missed information\n", PORT);
		close(client_sock);
		return 1;
	}
    snprintf(state, sizeof(state), "[%d][%s] Message received.\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 

	// Asignar dirección y nombre de archivo.
    snprintf(state, sizeof(state), "[%d][%s] Writing the file...\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 
	char *directory = (server_ip == S01_IP) ? "s01":"s02";
	char filename[BUFFER_SIZE-4], file[BUFFER_SIZE];
	sscanf(buffer, "%s\n", &filename);
	snprintf(file, sizeof(file), "%s/%s", directory, filename);
	// printf("archivo: %s\n", file);

	// Escribe el archivo
	char* text = strstr(buffer, "\n");
	FILE *fp;
    fp = fopen(file, "wb");
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return 1;
    }
	fputs(text, fp);
	fclose(fp);
    snprintf(state, sizeof(state), "[%d][%s] File successfully written.\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 
	
    snprintf(state, sizeof(state), "[%d][%s] Client petition completed with success.\n", PORT, get_time());
	printf("%s", state);
	strcat(message, state); 

	// send(client_sock, state, strlen(state), 0); 
    // sleep(1); // Peque~na pausa para evitar colision de datos
	

    send(client_sock, message, strlen(message), 0);
    sleep(1); // Peque~na pausa para evitar colision de datos
	close(client_sock);

	return 0;
}

int peticion_puerto(int *server_sock, int PORT) {
	int client_sock;
	struct sockaddr_in client_addr;
	socklen_t addr_size;

	// Espera y acepta una conexión entrante.
	addr_size = sizeof(client_addr);
	client_sock = accept(*server_sock, (struct sockaddr *)&client_addr,&addr_size);
	if (client_sock < 0) {
		perror("[-] Error on accept");
		return 1;
	}
    // Reasignar puerto    
    char message[50];
    sprintf(message, "%d", PORT);
    write(client_sock, message, strlen(message));
    close(client_sock);
	printf("[+][Server %d] Client reassigned to port %d...\n", SERVER_PORT, PORT);
	// Crear socket para escuchar en puerto nuevo
	int next_sock; 
	if(crear_socket(&next_sock, PORT) != 0) {
		return 1;
	}
    return next_sock;
}


int manejar_conexiones(int server_sock, int port) {
	// Conjunto de sockets
	fd_set current_sockets, ready_sockets;
	FD_ZERO(&current_sockets);
    int next_port = 1;
    int petition_sockets[100];
	int sock_size = 0;
    FD_SET(server_sock, &current_sockets);

	// Revisar los puertos constantemente
	while(true) {
		ready_sockets = current_sockets; // select es destructiva
		// Ver conexiones listas
		if(select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
			perror("[-] Error on select");
			return 1;
		}
		// Manejar peticiones a 49200
        if(FD_ISSET(server_sock, &ready_sockets)) {
            petition_sockets[next_port] = peticion_puerto(&server_sock, SERVER_PORT + next_port);
            FD_SET(petition_sockets[next_port], &current_sockets);
            sock_size++;
            if(next_port == 99) next_port = 1;
            else next_port++;
        }
		// Manejar peticiones a nuevos puertos
		for(int i=1; i<=sock_size; i++) {
			if(FD_ISSET(petition_sockets[i], &ready_sockets)) {
				manejar_peticion(&petition_sockets[i], SERVER_PORT+i);
			}
		}
	}
	return 0;
}



int main(int argc, char *argv[]) {
    // Verificar parámetros
    if (argc != 2) {
        printf("Type: %s <SERVER> \n", argv[0]);
        return 1;
    }
    if(strcmp(argv[1], "s01") == 0) server_ip = S01_IP;
    if(strcmp(argv[1], "s02") == 0) server_ip = S02_IP;
    if(server_ip == SERVER_IP) {
        printf("Invalid name of server: %s \n", argv[1]);
        return 1;
    }


	// Crear socket para escuchar en puerto 49200
	int server_sock; 
	if(crear_socket(&server_sock, SERVER_PORT) != 0) {
		return 1;
	}

	// Manejar peticiones del cliente
	manejar_conexiones(server_sock, SERVER_PORT);
	return 0;
    
}