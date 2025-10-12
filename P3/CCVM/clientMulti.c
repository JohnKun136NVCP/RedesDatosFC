#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTS {49200, 49201, 49202}
#define BUFFER_SIZE 1024

char* read_file(const char *filename, char *buffer, size_t buffer_size) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("[-] No se puede abrir el archivo");
		return NULL;
	}
	size_t read_size = fread(buffer, 1, buffer_size - 1, fp);
    	buffer[read_size] = '\0';

	fclose(fp);
	return buffer;
}


void send_and_receive_message(int server_port, int client_sock, const char *filename, const char *shift) {
	char file_buffer[BUFFER_SIZE * 10];
	char recv_buffer[BUFFER_SIZE * 10];
	char mensaje[BUFFER_SIZE * 10]; // Just in case
	char *file_content = read_file(filename, file_buffer, sizeof(file_buffer));
	snprintf(mensaje, sizeof(mensaje), "%s|%d|%s", file_content, server_port, shift);

	send(client_sock, mensaje, strlen(mensaje), 0);

	int bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
		if (bytes > 0) {
			recv_buffer[bytes] = '\0';
			printf("[*] SERVER RESPONSE %d: %s\n", server_port, recv_buffer);
		}
}    


// Funcion para crear sockets, esto es usado para crear 3 sockets en la practica, con el servidor
int create_socket(int port, const char *server_ip) {
	int client_sock;
	struct sockaddr_in serv_addr;
	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if (client_sock == -1) {
		perror("[-] Error to create the socket");
		return -1;
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(server_ip);
	if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
	< 0) {
		perror("[-] Error to connect");
		close(client_sock);
	} 
	return client_sock;
}

int main(int argc, char *argv[]) {
	if (argc <  6) {
		printf("Type: %s <servidor_ip> <port|s> <archivo|s> <desplazamiento> \n", argv[0]);
		return 1;
	}
	
	int ports[20];
	char *filenames[100];
	int port_count = 0;
	int file_count = 0;

	// Esto se hace para poder definir los puertos y los archivos de texto pasados como input
	for (int i = 2; i < (argc - 1); i++) {
		int value = atoi(argv[i]);
		if (value == 0) {
			filenames[file_count] = argv[i];
			file_count++;
		} else {
			ports[port_count] = value;
			port_count++;
		}
	}
	char *server_ip = argv[1];
	char *shift = argv[argc-1];
	
	// Dado que ambos, ports y archivos estan mapeados 1 a 1 en ambos arreglos, solo hace falta recorrer
	// cualquiera de los arreglos y usar el i para ambos
	for (int i = 0; i < port_count; i++) {
		int client_sock = create_socket(ports[i], server_ip);
		send_and_receive_message(ports[i], client_sock, filenames[i], shift);
		
		close(client_sock);
	}

	return 0;
}
