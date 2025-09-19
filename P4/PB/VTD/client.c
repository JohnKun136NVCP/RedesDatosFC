#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#define BUFFER_SIZE 4095

//Registra la fecha y hora
void logMessage(const char *msg){
	FILE *f = fopen("cliente_log.txt", "a");
	if (!f) return;
	time_t now = time(NULL);
	char timestamp[64];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(f, "[%s] %s\n", timestamp, msg);
	fclose(f);
}

//Conecta al servidor
int connectServer(const char *host, int port){
	int sockfd;
	struct hostent *he;
	struct sockaddr_in servaddr;

	if((he = gethostbyname(host)) == NULL){
		fprintf(stderr, "[-] Error resolviendo host %s\n", host);
		return -1;
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("[-] Error creando socket");
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("[-] Error en connect");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

//Envia el archivo indicado
void sendFile(const char *alias, const char *host, int port, const char *filename){
	int sockfd = connectServer(host, port);
	if (sockfd < 0) return;

	FILE *fp = fopen(filename, "r");
	if(!fp){
		perror("[-] Error abriendo archivo");
		close(sockfd);
		return;
	}

	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	snprintf(buffer, sizeof(buffer), "%s\n%s\n", alias, filename);
	fread(buffer + strlen(buffer), 1, sizeof(buffer) - strlen(buffer) - 1, fp);

	write(sockfd, buffer, strlen(buffer));

	memset(buffer, 0, BUFFER_SIZE);
	read(sockfd, buffer, sizeof(buffer));
	printf("[%s] Respuesta del servidor: %s\n", alias, buffer);
	logMessage(buffer);

	close(sockfd);
}

//Revisa el status del cliente
void checkStatus(const char *alias, int port, int num_targets, char *targets[]){
	for(int i = 0; i < num_targets; i++){
		int sockfd = connectServer(targets[i], port);
		if(sockfd < 0){
			printf("[STATUS] %s -> %s: OFFLINE\n", alias, targets[i]);
			continue;
		}

		char buffer[BUFFER_SIZE];
		snprintf(buffer, sizeof(buffer), "%s\n", targets[i]);
		write(sockfd, buffer, strlen(buffer));

		memset(buffer, 0, BUFFER_SIZE);
		read(sockfd, buffer, sizeof(buffer));

		printf("[STATUS] %s -> %s: %s\n", alias, targets[i], buffer);
		close(sockfd);
	}
}

int main(int argc, char *argv[]){
	if(argc < 2){
		fprintf(stderr, "Uso:\n");
		fprintf(stderr, " %s send <alias> <puerto> <archivo>\n", argv[0]);
		fprintf(stderr, " %s status <alias> <puerto> <servidor1>...\n", argv[0]);
		exit(1);
	}

	if(strcmp(argv[1], "send") == 0){
		if(argc != 5){
			fprintf(stderr, "Uso: %s send <alias> <puerto> <archivo>\n", argv[0]);
			exit(1);
		}
		char *alias = argv[2];
		int port = atoi(argv[3]);
		char *filename = argv[4];
		sendFile(alias, alias, port, filename);
	}else if(strcmp(argv[1], "status") == 0){
		if(argc < 5){
			fprintf(stderr, "Uso: %s status <alias> <puerto> <servidor1>...\n", argv[0]);
			exit(1);
		}
		char *alias = argv[2];
		int port = atoi(argv[3]);
		checkStatus(alias, port, argc - 4, &argv[4]);
	}else{
		fprintf(stderr, "Modo no valido %s\n", argv[1]);
		exit(1);
	}
	return 0;
}
