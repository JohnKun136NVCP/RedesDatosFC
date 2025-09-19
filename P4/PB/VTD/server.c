#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 49200
#define BUFFER_SIZE 4095
#define MAX_SERVERS 16

int main(int argc, char *argv[]){
	int sockfds[MAX_SERVERS];
	int num_servers = argc - 1;
	//Crea un socket para cada alias
	for(int i = 0; i < num_servers; i++){
		struct hostent *he = gethostbyname(argv[i + 1]);
		if(!he){
			fprintf(stderr, "[-] No se logra conectar %s\n", argv[i + 1]);
			exit(1);
		}
		if((sockfds[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			perror("[-] Error al crear el socket");
			exit(1);
		}

		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(PORT);
		//El alias se asocia a su IP
		memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

		if(bind(sockfds[i], (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			perror("[-] Error en bind");
			close(sockfds[i]);
			exit(1);
		}

		if(listen(sockfds[i], 5) < 0){
			perror("[-] Error en listen");
			close(sockfds[i]);
			exit(1);
		}

		printf("[+] Servidor en %s esuchando en puerto %d...\n", argv[i + 1], PORT);
	}

	fd_set readfds;
	int maxfd = -1;

	while(1){
		FD_ZERO(&readfds);
		for(int i = 0; i < num_servers; i++){
			FD_SET(sockfds[i], &readfds);
			if(sockfds[i] > maxfd) maxfd = sockfds[i];
		}

		if(select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0){
			perror("[-] Error en select");
			exit(1);
		}

		for(int i = 0; i < num_servers; i++){
			if(FD_ISSET(sockfds[i], &readfds)){
				struct sockaddr_in cliaddr;
				socklen_t len = sizeof(cliaddr);
				int newsockfd = accept(sockfds[i], (struct sockaddr *)&cliaddr, &len);
				if(newsockfd < 0){
					perror("[-] Error en accept");
					continue;
				}

				char buffer[BUFFER_SIZE];
				memset(buffer, 0, BUFFER_SIZE);
				read(newsockfd, buffer, BUFFER_SIZE);

				time_t now = time(NULL);
				char timestamp[64];
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

				char *token = strtok(buffer, "\n");
				char *destino = token ? token : "desconocido";

				token = strtok(NULL, "\n");
				char *filename = token;

				//El archivo se guarda en el directorio correspondiente al alias
				if(filename){
					char path[256];
					snprintf(path, sizeof(path), "%s/%s", destino, filename);
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

					printf("[%s] Archivo %s recibido y guardado en: %s\n", timestamp, filename, path);

					char msg[256];
					snprintf(msg, sizeof(msg), "Archivo %s recibido en %s", filename, destino);
					write(newsockfd, msg, strlen(msg));
				}else{
					printf("[%s] Cliente status conectado a %s\n", timestamp, argv[i + 1]);
					write(newsockfd, "OK", 2);
				}
				close(newsockfd);
			}
		}
	}
	for(int i = 0; i < num_servers; i++){
		close(sockfds[i]);
	}
	return 0;
}
