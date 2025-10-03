#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define PORT 49200
#define BUFFER_SIZE 4095
#define MAX_SERVERS 16

sem_t mutex;	//Semaforo para exclusion mutua
int turno = 0;
int num_servers = 0;

//Estructura que pasa informacion a cada hilo
typedef struct{
	int id;
	char alias[64];
	int sockfd;
} server_info;

//Funcion de cada hilo del servidor
void *serverThread(void *arg){
	server_info *info = (server_info *) arg;
	int id = info->id;

	while(1){
		while(turno != id){
			usleep(10000);
		}

		sem_wait(&mutex);
		printf("[Servidor %s] Turno asignado, listo para aceptar conexiones...\n", info->alias);

		struct sockaddr_in cliaddr;
		socklen_t len = sizeof(cliaddr);

		//Trata de aceptar conexiones
		int newsockfd = accept(info->sockfd, (struct sockaddr *)&cliaddr, &len);
		if(newsockfd < 0){
			perror("[-] Error en accept");
		} else {
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

			if(filename){
				char path[256];
				snprintf(path, sizeof(path), "%s/%s", destino, filename);
				FILE *fp = fopen(path, "w");
				if(!fp){
					perror("[-] Error al crear archivo");
					close(newsockfd);
				} else {
					char *content = strtok(NULL, "");
					if(content){
						fwrite(content, 1, strlen(content), fp);
					}
					fclose(fp);
					printf("[%s] Archivo recibido en %s\n", timestamp, filename, path);
					char msg[256];
					snprintf(msg, sizeof(msg), "Archivo %s recibido en %s", filename, destino);
					write(newsockfd, msg, strlen(msg));
				}
			} else {
				printf("[%s] Cliente status conectado a %s\n", timestamp, info->alias);
				write(newsockfd, "OK", 2);
			}
			close(newsockfd);
		}
		turno = (turno + 1) % num_servers;
		sem_post(&mutex);
	}
	return NULL;
}

int main(int argc, char *argv[]){
	if(argc < 2){
		fprintf(stderr, "Uso: %s <server1> <server2>...\n", argv[0]);
		exit(1);
	}

	num_servers = argc - 1;
	if(num_servers > MAX_SERVERS){
		fprintf(stderr, "[-] Numero maximo de servidores soportado: %d\n", MAX_SERVERS);
		exit(1);
	}

	sem_init(&mutex, 0, 1);

	pthread_t hilos[MAX_SERVERS];
	server_info infos[MAX_SERVERS];

	for(int i = 0; i < num_servers; i++){
		struct hostent *he = gethostbyname(argv[i + 1]);
		if(!he){
			fprintf(stderr, "[-] No se puede resolver %s\n", argv[i + 1]);
			exit(1);
		}

		int sockfd;
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			perror("[-] Error al crear socket");
			exit(1);
		}

		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(PORT);
		memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

		if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			perror("[-] Error en listen");
			close(sockfd);
			exit(1);
		}

		if(listen(sockfd, 5) < 0){
			perror("[-] Error en listen");
			close(sockfd);
			exit(1);
		}

		printf("[+] Servidor %s escuchando en puerto %d...\n", argv[i + 1], PORT);

		infos[i].id = i;
		infos[i].sockfd = sockfd;
		strncpy(infos[i].alias, argv[i + 1], sizeof(infos[i].alias) - 1);
		pthread_create(&hilos[i], NULL, serverThread, &infos[i]);
	}
	for(int i = 0; i < num_servers; i++){
		pthread_join(hilos[i], NULL);
	}
	sem_destroy(&mutex);
	return 0;
}
