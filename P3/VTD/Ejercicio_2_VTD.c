#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

//Lee archivo
void readFileContent(const char *filename, char *buffer){
	FILE *fp = fopen(filename, "r");
	if(!fp){
		perror("[-] Error al abrir archivo de texto");
		exit(1);
	}
	fread(buffer, 1, BUFFER_SIZE, fp);
	fclose(fp);
}

//Envia al server
void sendToServer(const char *ip, int port, const char *text){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	if(sockfd < 0){
		perror("[-] Error creando socket");
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &server_addr.sin_addr);
	
	if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("[-] Error al conectar con el servidor");
		close(sockfd);
		return;
	}
	write(sockfd, text, strlen(text));
	char buffer[BUFFER_SIZE] = {0};
	read(sockfd, buffer, BUFFER_SIZE);
	printf("Respuesta del servidor en puerto %d: %s\n", port, buffer);
	close(sockfd);
}

int main(int argc, char *argv[]){
	if(argc < 5){
		fprintf(stderr, "Uso %s <IP> <PUERTO1> ... <PUERTON> <ARCHIVO1> ...<ARCHIVON > <SHIFT>\n", argv[0]);
		exit(1);
	}
	char *serverIP = argv[1];
	//Numero de puertos
	int totalPorts = argc - 3;
	int n = totalPorts / 2;
	if(totalPorts % 2 != 0){
		fprintf(stderr, "[-] Error: Debe haber la misma cantidad de puertos y archivos\n");
		exit(1);
	}
	int shift = atoi(argv[argc - 1]);
	for(int i = 0; i < n; i++){
		int port = atoi(argv[2 + i]);
		char *filename = argv[2 + n + i];
		char buffer[BUFFER_SIZE] = {0};
		readFileContent(filename, buffer);
		sendToServer(serverIP, port, buffer);
	}
	return 0;
}
