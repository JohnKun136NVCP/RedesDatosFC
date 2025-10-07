#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

// Funcion auxiliar para poder escribir el estado mandado por el servidor
void write_log(const char *status) {
	FILE *fp = fopen("client_log.txt", "a");
	if (fp == NULL) {
		perror("[-] Error opening log file");
		return;
	}
	time_t rn = time(NULL);
	struct tm *t = localtime(&rn);
	char timestamp[30];
	// Para darle el formato a la fecha y hora
	strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H:%M:%S", t);
	fprintf(fp, "[%s] %s\n", timestamp, status);
	fclose(fp);
}

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


void send_and_receive_message(int server_port, int client_sock, const char *filename) {
	char file_buffer[BUFFER_SIZE * 10];
	char recv_buffer[BUFFER_SIZE * 10];
	char mensaje[BUFFER_SIZE * 10]; // Just in case
	
	// Esto es provisional, para recibir primero el estado del servidor
	// antes de mandar cualquier archivo
	int bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
	if (bytes > 0) {
		recv_buffer[bytes] = '\0';
		printf("[*][Server on port %d] Response: %s\n", server_port, recv_buffer);
		write_log(recv_buffer);
	}
	char *file_content = read_file(filename, file_buffer, sizeof(file_buffer));
	snprintf(mensaje, sizeof(mensaje), "%s|%d|%s", file_content, server_port, filename);

	send(client_sock, mensaje, strlen(mensaje), 0);

	// Recibir el estado del servidor una vez mandado el archivo
	bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
		if (bytes > 0) {
			recv_buffer[bytes] = '\0';
			printf("[*][Server on port %d] Response: %s\n", server_port, recv_buffer);
			write_log(recv_buffer);
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

// Funcion para obtener la direccion ip dado un alias
char* get_ip(const char* alias) {
        struct addrinfo hints, *result;
        static char ip_str[INET_ADDRSTRLEN];
                
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
         
        int status = getaddrinfo(alias, NULL, &hints, &result);
        if (status != 0) {
                fprintf(stderr, "[-] Error: alias not resolved %s: %s\n", alias, gai_strerror(status));
                return NULL;
        }
        struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
        inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
        freeaddrinfo(result);
        return ip_str;
}

int main(int argc, char *argv[]) {
	if (argc <  3) {
		printf("Type: %s <servidor_ip> <port> <archivo|s> \n", argv[0]);
		return 1;
	}
	
	char *server_ip = get_ip(argv[1]);
	int port = atoi(argv[2]);
	for (int i = 3; i < argc; i++) {
		char *filename = argv[i];
		printf("\n[*] Sending file: %s\n", filename);
		
		int client_sock = create_socket(port, server_ip);
		if (client_sock == -1) {
			continue;
		}
		
		send_and_receive_message(port, client_sock, filename);
		close(client_sock);
	}
	return 0;
}
