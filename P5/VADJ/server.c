#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 2048
#define NUM_SERVERS 4

typedef struct {
	char* alias;
	char* ip;
	int token_port;
} ServerInfo;

ServerInfo servers[NUM_SERVERS] = {
	{"s01", "192.168.100.101", 9091 },
	{"s02", "192.168.100.102", 9092 },
	{"s03", "192.168.100.103", 9093 },
	{"s04", "192.168.100.104", 9094 },
};

pthread_mutex_t token_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int has_token = 0;
volatile int pass_token = 0;

typedef struct {
	int id;
	int port;
} ThreadArgs;

void* manejador_cliente(void* arg) {

	ThreadArgs* args = (ThreadArgs*)arg;

	int id = args->id;
	int port = args->port;
	char save_dir[100];

	snprintf(save_dir, sizeof(save_dir), "%s/%s", getenv("HOME"), servers[id].alias);

	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size = sizeof(client_addr);

	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1) {
		perror("[-] Error al crear el socket");
		return NULL;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(servers[id].ip);

	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		fprintf(stderr, "[-] Error en bind para %s:%d\n", servers[id].ip, port);
		return NULL;
	}

	if (listen(server_sock, 5) < 0) {
        	perror("[-] Error en listen");
        	return NULL;
    	}

	while (1) {

		while(!has_token) {
			sleep(1);
		}

		client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
	        if (client_sock < 0) {
            		if (errno == EWOULDBLOCK || errno == EAGAIN) {
				printf("[-] No se conecto ningun cliente al server %s. Cediendo el turno.\n", servers[id].alias);
			} else {
				perror("[-] Error en accept");
        	}
		} else {

        		char client_ip[INET_ADDRSTRLEN];
        		inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        		printf("\n[+] Conexión aceptada desde %s\n", client_ip);

	        	char buffer[BUFFER_SIZE] = {0};
        		int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        		if (bytes_read > 0) {
            			char *filename = strtok(buffer, "\n");
            			char *file_content = filename + strlen(filename) + 1;

		        	if (filename && file_content) {
                			char output_filepath[256];
                			snprintf(output_filepath, sizeof(output_filepath), "%s/%s", save_dir, filename);

			                FILE *fp = fopen(output_filepath, "w");
        	        		if (fp == NULL) {
                	    			perror("[-] Error al crear el archivo de salida");
                    				send(client_sock, "ERROR_ARCHIVO", strlen("ERROR_ARCHIVO"), 0);
                			} else {
                    				fprintf(fp, "%s", file_content);
                    				fclose(fp);
                    				printf("[+] Archivo guardado en '%s'\n", output_filepath);
                    				send(client_sock, "PROCESADO", strlen("PROCESADO"), 0);
                			}
            			} else {
                 			send(client_sock, "ERROR_FORMATO", strlen("ERROR_FORMATO"), 0);
            			}
        		} else {
            			fprintf(stderr, "[-] Error al recibir datos del cliente.\n");
        		}
        		close(client_sock);
        		printf("[+] Conexión con %s cerrada.\n", client_ip);
    		}
		pthread_mutex_lock(&token_mutex);
		pass_token = 1;
		pthread_mutex_unlock(&token_mutex);
	}

	close(server_sock);
	return NULL;

}

void* manejador_token(void* arg) {
	int id = *(int*)arg;
	int next_server = (id + 1) % NUM_SERVERS;
    	int token_listen_sock;
	struct sockaddr_in token_addr;

	token_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    	token_addr.sin_family = AF_INET;
    	token_addr.sin_port = htons(servers[id].token_port);
    	token_addr.sin_addr.s_addr = inet_addr(servers[id].ip);

	bind(token_listen_sock, (struct sockaddr *)&token_addr, sizeof(token_addr));
    	listen(token_listen_sock, 5);
    	printf("[+] Servidor '%s' listo para recibir token en el puerto %d\n", servers[id].alias, servers[id].token_port);

    	while(1) {

		if (!has_token) {
            		int prev_sock = accept(token_listen_sock, NULL, NULL);
            		if (prev_sock >= 0) {
                		char token_buffer[10];
                		recv(prev_sock, token_buffer, sizeof(token_buffer), 0);
                		if (strncmp(token_buffer, "TOKEN", 5) == 0) {
                    			printf("[TOKEN] Token recibido.\n");
                    			pthread_mutex_lock(&token_mutex);
                    			has_token = 1;
                    			pass_token = 0;
                    			pthread_mutex_unlock(&token_mutex);
                		}
                		close(prev_sock);
            		}
        	}

        	if (has_token) {
            		while (!pass_token) {
                		sleep(1);
            		}

	       		printf("[TOKEN] Pasando el token a '%s'...\n", servers[next_server].alias);

            		int next_server_sock = socket(AF_INET, SOCK_STREAM, 0);
            		struct sockaddr_in next_server_addr;
            		next_server_addr.sin_family = AF_INET;
            		next_server_addr.sin_port = htons(servers[next_server].token_port);
            		next_server_addr.sin_addr.s_addr = inet_addr(servers[next_server].ip);

	            	while (connect(next_server_sock, (struct sockaddr *)&next_server_addr, sizeof(next_server_addr)) < 0) {
                		perror("[-] Error conectando para pasar token, reintentando");
                		sleep(2);
            		}

            		send(next_server_sock, "TOKEN", strlen("TOKEN"), 0);
            		close(next_server_sock);
            		pthread_mutex_lock(&token_mutex);
            		has_token = 0;
            		pass_token = 0;
            		pthread_mutex_unlock(&token_mutex);
        	}
    	}
    	return NULL;
}

int main(int argc, char *argv[]) {

    	if (argc < 3) {
        	fprintf(stderr, "Uso: %s <alias: s01|s02|...> <puerto>\n", argv[0]);
        	return 1;
    	}

    	char *alias = argv[1];
    	int port = atoi(argv[2]);
	int id = -1;

	for (int i = 0; i < NUM_SERVERS; i++) {
		if (strcmp(alias, servers[i].alias) == 0) {
			id = i;
			break;
		}
	}

	if (id == -1) {
		fprintf(stderr, "[-] Alias no reconocido: %s.\n", alias);
		return 1;
	}

	if (id == 0) {
		has_token = 1;
	}

	pthread_t client_tid, token_tid;

	ThreadArgs client_args;
	client_args.id = id;
	client_args.port = port;

	pthread_create(&client_tid, NULL, manejador_cliente, &client_args);
	pthread_create(&token_tid, NULL, manejador_token, &id);

	pthread_join(client_tid, NULL);
	pthread_join(token_tid, NULL);

	pthread_mutex_destroy(&token_mutex);

	return 0;
}
