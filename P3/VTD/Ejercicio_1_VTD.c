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

//Crea un socket en un puerto
int createListenSocket(int port){
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0){
		perror("[-] Error al crear el socket");
		exit(1);
	}

	//En caso de reiniciar el server
	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		perror("[-] Error en bind");
		close(server_socket);
		exit(1);
	}

	if(listen(server_socket, 5) < 0){
		perror("[-] Error en listen");
		close(server_socket);
		exit(1);
	}
	printf("[+] Servidor escuchando al puerto %d...\n", port);
	return server_socket;
}

int main(int argc, char *argv[]){
	if(argc < 2){
		fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
		exit(1);
	}
	int server_sockets[argc-1];
	int max_fd = 0;
	
	//Crea un socket para cada puerto
	for(int i = 1; i < argc; i++){
		int port = atoi(argv[i]);
		server_sockets[i-1] = createListenSocket(port);
		if(server_sockets[i-1] > max_fd){
			max_fd = server_sockets[i-1];
		}
	}
	fd_set readfds;
	char buffer[BUFFER_SIZE];
	while(1){
		FD_ZERO(&readfds);
		for(int i = 0; i < argc - 1; i++){
			FD_SET(server_sockets[i], &readfds);
		}
		int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
		if(activity < 0){
			perror("[-] Error en select");
			exit(1);
		}
		for(int i = 0; i < argc - 1; i++){
			if(FD_ISSET(server_sockets[i], &readfds)){
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_socket = accept(server_sockets[i], (struct sockaddr*)&client_addr, &client_len);
				if(client_socket < 0){
					perror("[-] Error en accept");
					continue;
				}
				memset(buffer, 0, BUFFER_SIZE);
				read(client_socket, buffer, BUFFER_SIZE);
				printf("[+] Mensaje recibido en puerto %d: %s\n", ntohs(client_addr.sin_port), buffer);
				encryptCaesar(buffer, 3);
				write(client_socket, buffer, strlen(buffer));
				close(client_socket);
			}
		}
	}
	return 0;
}
