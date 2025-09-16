#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 1024

//Lee un archivo
void readFileContent(const char *filename, char *buffer, size_t size){
	FILE *fp = fopen(filename, "r");
	if(!fp){
		perror("Error al abrir archivo de texto");
		exit(EXIT_FAILURE);
	}
	size_t bytesRead = fread(buffer, 1, size - 1, fp);
	buffer[bytesRead] = '\0';
	fclose(fp);
}

//Conecta al server
int connectToServer(const char *ip, int port){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("Error al crear socket");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Error al conectar con el servidor");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

//Envia el archivo encriptado
void sendFileReceiveResponse(const char *ip, int serverPort, int targetPort, int shift, const char *text){
	int sockfd = connectToServer(ip, serverPort);
	if(sockfd < 0) return;
	
	char message[BUFFER_SIZE];
	snprintf(message, sizeof(message), "%d %d %s", targetPort, shift, text);
	send(sockfd, message, strlen(message), 0);
	char buffer[BUFFER_SIZE];
	int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
	if(bytes > 0){
		buffer[bytes] = '\0';
		printf("[Servidor %d] %s\n]", serverPort, buffer);
		char filename[64];
		snprintf(filename, sizeof(filename), "respuesta_%d.txt", serverPort);
		FILE *fp = fopen(filename, "w");
		if(fp){
			fwrite(buffer, 1, bytes, fp);
			fclose(fp);
			printf("Respuesta guardada en '%s'\n", filename);
		}
	}
}

int main(int argc, char *argv[]){
	if(argc != 5){
		fprintf(stderr, "Uso %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <ARCHIVO_TEXTO>\n", argv[0]);
		return 1;
	}
	const char *serverIP = argv[1];
	int targetPort = atoi(argv[2]);
	int shift = atoi(argv[3]);
	const char *fileName = argv[4];
	char text[BUFFER_SIZE];
	readFileContent(fileName, text, sizeof(text));
	int serverPorts[] = {49200, 49201, 49202};
	for(int i = 0; i < 3; i++){
		sendFileReceiveResponse(serverIP, serverPorts[i], targetPort, shift, text);
	}
	return 0;
}
