#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define NUM_PORTS 3

void cryptCaesar (char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' + shift + 26) % 26 ) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' + shift + 26) % 26 ) + 'a';
		} else {
			text[i] = c;
		}
	}
}

void toLowerCase (char *str) {
	for (int i = 0; str[i]; i++)
		str[i] = tolower((unsigned char) str[i]);
}

void trim (char *str) {
	char *end;
	while (isspace ((unsigned char) *str)) str++;
	end = str + strlen(str) - 1;
	while (end > str && isspace ((unsigned char)*end)) end--;
	* (end + 1) = '\0';
}

int main(int argc, char *argv[]) {

	int ports[NUM_PORTS] = {49200, 49201, 49202};
	int server_sockets[NUM_PORTS];
	struct sockaddr_in server_addrs[NUM_PORTS];

	for (int i = 0; i < NUM_PORTS; i++) {
		server_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (server_sockets[i] == -1) {
			perror("[-] Error al crear el socket");
			return 1;
		}

		server_addrs[i].sin_family = AF_INET;
		server_addrs[i].sin_port = htons(ports[i]);
		server_addrs[i].sin_addr.s_addr = INADDR_ANY;

		if (bind (server_sockets[i], (struct sockaddr *)&server_addrs[i], sizeof(server_addrs[i])) < 0) {
			fprintf(stderr, "[-] Error en bind para el puerto %d\n", ports[i]);
			return 1;
		}

		if (listen (server_sockets[i], 5) < 0) {
			perror("[-] Error on listen");
			return 1;
		}
		printf("[+] Servidor escuchando en el puerto %d...\n", ports[i]);
	}

	fd_set read_fds;
	int max_sd = 0;

	for(int i = 0; i < NUM_PORTS; i++) {
		if (server_sockets[i] > max_sd) {
			max_sd = server_sockets[i];
		}
	}

	printf("\n[+] Servidor optimizado listo. Esperando conexiones...\n");

	while(1) {
		FD_ZERO(&read_fds);
		for (int i = 0; i < NUM_PORTS; i++) {
			FD_SET(server_sockets[i], &read_fds);
		}
		int actividad = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
		if (actividad < 0) {
			perror("[-] Error on select");
		}

		for (int i = 0; i < NUM_PORTS; i++) {
			if (FD_ISSET (server_sockets[i], &read_fds)) {
				int client_sock;
				struct sockaddr_in client_addr;
				socklen_t addr_size = sizeof(client_addr);

				client_sock = accept (server_sockets[i], (struct sockaddr *)&client_addr, &addr_size);

				if (client_sock < 0) {
					perror("[-] Error on accept");
					continue;
				}

				char client_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
				printf("\n[+] Conexion aceptada en el puerto %d desde %s\n", ports[i], client_ip);

				char buffer[BUFFER_SIZE] = {0};

				int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

				if (bytes_read > 0) {
					buffer[bytes_read] = '\0';
					int requested_port, shift;
					char file_content[BUFFER_SIZE];

					sscanf (buffer, "%d %d %[^\n]", &requested_port, &shift, file_content);

					if (requested_port != ports[i]) {
						printf("[+] [PUERTO %d] Peticion RECHAZADA (cliente pidio puerto %d)\n", ports[i], requested_port);
						send(client_sock, "RECHAZADO", strlen("RECHAZADO"), 0);
					} else {
						printf("[+] [PUERTO %d] Peticion ACEPTADA, Cifrando archivo \n", ports[i]);
						cryptCaesar(file_content, shift);
						//printf("Contenido cifrado:\n %s\n", file_content);

						char output_filename[50];
						snprintf(output_filename, sizeof(output_filename), "file_%d_cesar.txt", ports[i]);
						FILE *fp = fopen(output_filename, "w");
						if (fp == NULL ) {
							perror("[-] Error al crear el archivo de salida");
						} else {
							fprintf(fp, "%s", file_content);
							fclose(fp);
							printf("[+] Contenido cifrado guardado en '%s'\n", output_filename);
						}

						send(client_sock, "PROCESADO", strlen("PROCESADO"),0);
					}
				} else {
					fprintf(stderr, "[-] Error al recibir datos del cliente en el puerto %d\n", ports[i]);
				}
				close(client_sock);
				printf("[+] Conexion con %s en el puerto %d cerrada.\n", client_ip, ports[i]);
			}
		}
	}

	for (int i = 0; i < NUM_PORTS; i++) {
		close(server_sockets[i]);
	}

	return 0;
}
