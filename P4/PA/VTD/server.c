#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT 49200
#define BUFFER_SIZE 4095

int main(int argc, char *argv[]){
	int sockfd, newsockfd;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t len;
	char buffer[BUFFER_SIZE];
	time_t now;

	if(argc < 2){
		fprintf(stderr, "Uso: %s <server1>...\n", argv[0]);
		exit(1);
	}

	//Crea un socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("[-] Error al crear socket");
		exit(1);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);

	if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("[-] Error en bind");
		close(sockfd);
		exit(1);
	}
	
	if(listen(sockfd, 5) < 0){
		perror("[-] Error en listen");
		close(sockfd);
		exit(1);
	}
	printf("[+] Servidor esperando conexiones en puerto %d...\n", PORT);

	len = sizeof(cliaddr);
	while((newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len))){
		if(newsockfd < 0){
			perror("[-] Error en accept");
			continue;
		}
		memset(buffer, 0, BUFFER_SIZE);
		read(newsockfd, buffer, BUFFER_SIZE);
		//Registra la fecha y hora
		now = time(NULL);
		char timestamp[64];
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
		char *token = strtok(buffer, "\n");
		//Verifica el directorio del alias
		char *directorio = NULL;
		for(int i = 1; i < argc; i++){
			if(strcmp(token, argv[i]) == 0){
				printf("[%s] Cliente conectado: %s\n", timestamp, token);
				write(newsockfd, "Servidor en espera", 18);
				directorio = argv[i];
				break;
			}
		}

		if(!directorio){
			directorio = argv[1];
		}else{
			token = strtok(NULL, "\n");
		}

		if(token){
			char *filename = token;
			char path[256];
			snprintf(path, sizeof(path), "%s/%s", argv[1], filename);
			FILE *fp = fopen(path, "w");
			if(!fp){
				perror("[-] Error al crear archivo");
				close(newsockfd);
				continue;
			}
			char *content = strtok(NULL, "");
			if(content){
				fwrite(content, 1, strlen(content), fp);
			}
			fclose(fp);
			printf("[%s] Archivo recibido y guardado en: %s\n", timestamp, path);
			char msg[256];
			snprintf(msg, sizeof(msg), "Archivo %s recibido en %s", filename, argv[1]);
			write(newsockfd, msg, strlen(msg));
		}
		close(newsockfd);
	}
	close(sockfd);
	return 0;
}
