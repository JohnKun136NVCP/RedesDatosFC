#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]){
	int sockfd;
	char buffer[BUFFER_SIZE];
	time_t now;

	if(argc < 3){
		fprintf(stderr, "Uso: %s <server_ip/alias> <puerto> <archivo.txt>\n", argv[0]);
		exit(1);
	}

	char *server = argv[1];
	int port = atoi(argv[2]);

	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	char portstr[10];
	sprintf(portstr, "%d", port);

	int status = getaddrinfo(server, portstr, &hints, &res);
	if(status != 0){
		fprintf(stderr, "[-] Error en getadrrinfo: %s\n", gai_strerror(status));
		exit(1);
	}
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(sockfd < 0){
		perror("[-] Error al crear socket");
		freeaddrinfo(res);
		exit(1);
	}

	if(connect(sockfd, res->ai_addr, res->ai_addrlen) < 0){
		perror("[-] Error en connect");
		freeaddrinfo(res);
		close(sockfd);
		exit(1);
	}
	freeaddrinfo(res);

	if(argc == 4){
		char *filename = argv[3];
		FILE *fp = fopen(filename, "r");
		if(!fp){
			perror("[-] Error al abrir el archivo");
			close(sockfd);
			exit(1);
		}
		//Envia alias
		write(sockfd, server, strlen(server));
		write(sockfd, "\n", 1);
		//Envia nombre del archivo
		write(sockfd, filename, strlen(filename));
		write(sockfd, "\n", 1);
		//Envia contenido del archivo
		while(!feof(fp)){
			size_t n = fread(buffer, 1, BUFFER_SIZE, fp);
			if(n > 0){
				write(sockfd, buffer, n);
			}
		}
		fclose(fp);
	} else {
		//Envia mensaje
		write(sockfd, server, strlen(server));
	}

	memset(buffer, 0, BUFFER_SIZE);
	read(sockfd, buffer, BUFFER_SIZE);
	//Registra la fecha y hora
	now = time(NULL);
	char timestamp[64];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

	FILE *f = fopen("cliente_log.txt", "a");
	if(f){
		fprintf(f, "[%s] %s\n", timestamp, buffer);
		fclose(f);
	}
	printf("[%s] Respuesta del servidor: %s\n", timestamp, buffer);
	close(sockfd);
	return 0;
}
