#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4095

//Envia un archivo a un servidor especifico
void sendFile(const char *server, int port, const char *filename){
	int sockfd;
	struct sockaddr_in servaddr;
	struct hostent *he;
	he = gethostbyname(server);

	if(!he){
		fprintf(stderr, "[-] No se puede resolver %s\n", server);
		exit(1);
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("[-] Error al crear socket");
		exit(1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("[-] Error en connect");
		close(sockfd);
		exit(1);
	}

	FILE *fp = fopen(filename, "r");
	if(!fp){
		perror("[-] Error al abrir archivo");
		close(sockfd);
		exit(1);
	}

	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	snprintf(buffer, sizeof(buffer), "%s\n%s\n", server, filename);
	size_t len = strlen(buffer);
	fread(buffer + len, 1, sizeof(buffer) - len, fp);
	fclose(fp);

	write(sockfd, buffer, strlen(buffer));

	memset(buffer, 0, BUFFER_SIZE);
	read(sockfd, buffer, BUFFER_SIZE);
	printf("[RESPUESTA] %s\n", buffer);

	close(sockfd);
}

//Verifica el status de los demas servidores
void checkStatus(const char *alias, int port, int num_servers, char *servers[]){
	for(int i = 0; i < num_servers; i++){
		const char *server = servers[i];
		if(strcmp(server, alias) == 0) continue;

		int sockfd;
		struct sockaddr_in servaddr;
		struct hostent *he;

		he = gethostbyname(server);
		if(!he){
			fprintf(stderr, "[-] No se puede resolver %s\n", server);
			continue;
		}

		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			perror("[-] Error al crear socket");
			continue;
		}

		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
		memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

		if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			printf("[-] Servidor %s no disponible\n", server);
			close(sockfd);
			continue;
		}

		char buffer[BUFFER_SIZE];
		snprintf(buffer, sizeof(buffer), "%s\n", server);
		write(sockfd, buffer, strlen(buffer));

		memset(buffer, 0, BUFFER_SIZE);
		read(sockfd, buffer, BUFFER_SIZE);

		printf("[STATUS] %s -> %s\n", server, buffer);

		close(sockfd);
	}
}

int main(int argc, char *argv[]){
	if(argc < 5){
		fprintf(stderr, "Uso:\n");
		fprintf(stderr, " %s send <alias destino> <puerto> <archivo>\n", argv[0]);
		fprintf(stderr, " %s status <alias origen> <puerto> <server2> <server3>...\n", argv[0]);
		exit(1);
	}

	if(strcmp(argv[1], "send") == 0){
		if(argc != 5){
			fprintf(stderr, "Uso: %s send <alias destino> <puerto> <archivo>\n", argv[0]);
			exit(1);
		}
		sendFile(argv[2], atoi(argv[3]), argv[4]);
	} else if(strcmp(argv[1], "status") == 0){
		if(argc < 5){
			fprintf(stderr, "Uso: %s status <alias origen> <puerto> <server2>...\n", argv[0]);
			exit(1);
		}
		checkStatus(argv[2], atoi(argv[3]), argc - 4, &argv[4]);
	} else{
		fprintf(stderr, "Modo invalido. Use 'send' o 'status'.\n");
		exit(1);
	}
	return 0;
}
