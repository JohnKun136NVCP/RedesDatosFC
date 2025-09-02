#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BACKLOG 5
#define BUFFER_SIZE 4095

//Cifrado Cesar
void encryptCaesar(char *text, int shift){
	shift = shift % 26;
	for(int i = 0; text[i] != '\0'; i++){
		char c = text[i];
		if(isupper(c)){
			text[i] = ((c - 'A' + shift) % 26) + 'A';
		}else if(islower(c)){
			text[i] = ((c - 'a' + shift) % 26) + 'a';
		}else{
			text[i] = c;
		}
	}
}

int createListenSocket(int port){
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket == -1){
		perror("[-] Error al crear el socket");
		exit(EXIT_FAILURE);
	}

	//En caso de reiniciar el server
	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		perror("[-] Error en bind");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	if(listen(server_socket, BACKLOG) < 0){
		perror("[-] Error en listen");
		close(server_socket);
		exit(EXIT_FAILURE);
	}
	printf("[+] Servidor escuchando al puerto %d...\n", port);
	return server_socket;
}

int receiveRequest(int client_socket, int *target_port, int *shift, char *text, size_t text_sz){
	char buffer[BUFFER_SIZE];
	int n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
	if(n <= 0){
		return 0;
	}
	buffer[n] = '\0';

	int matched = sscanf(buffer, "%d %d %[^\n]", target_port, shift, text);
	
	if(matched < 2){
		return 0;
	}else if(matched == 2){
		if(text_sz > 0) text[0] = '\0';
	}else{
		text[text_sz - 1] = '\0';
	}
	return 1;
}

void handleClient(int client_socket, int my_port){
	int target_port = 0;
	int shift = 0;
	char text[BUFFER_SIZE];

	if(!receiveRequest(client_socket, &target_port, &shift, text, sizeof(text))){
		const char *msg = "Error: solicitud invalida\n";
		send(client_socket, msg, strlen(msg), 0);
		close(client_socket);
		return;
	}
	
	printf("[Servidor %d] Solicitud: target_port=%d, shift=%d\n", my_port, target_port, shift);
	if(target_port == my_port){
		encryptCaesar(text, shift);
		char response_header[128];
		int hdr_len = snprintf(response_header, sizeof(response_header), "Archivo procesado en puerto %d:\n", my_port);
		send(client_socket, response_header, hdr_len, 0);
		send(client_socket, text, strlen(text), 0);
		send(client_socket, "\n", 1, 0);
		printf("[Servidor %d] Texto cifrado enviado\n", my_port);
	}else{
		char response[128];
		int len = snprintf(response, sizeof(response), "Soliciutd rechazada por servidor en puerto %d\n", my_port);
		send(client_socket, response, len, 0);
		printf("[Servidor %d] Solicitud rechazada (target=%id)\n", my_port, target_port);

	}
	close(client_socket);
}

int main(int argc, char *argv[]){
	if(argc != 2){
		fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
		return EXIT_FAILURE;
	}
	int my_port = atoi(argv[1]);
	int server_socket = createListenSocket(my_port);
	while(1){
		struct sockaddr_in client_addr;
		socklen_t addrlen = sizeof(client_addr);
		int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addrlen);
		if(client_socket < 0){
			perror("[-] Error en accept");
			continue;
		}
		printf("[+] Cliente conectado\n");
		handleClient(client_socket, my_port);
	}
	close(server_socket);
	return EXIT_SUCCESS;
}
