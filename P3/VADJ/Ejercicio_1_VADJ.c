#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

void conectar_enviar (const char *server_ip, int port, int shift, const char *file_content, int port_msg) {

	int client_sock;
	struct sockaddr_in serv_addr;
	char buffer[BUFFER_SIZE] = {0};
	char mensaje[BUFFER_SIZE * 2];

	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock == -1) {
		perror("[-] Error to create the socket");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(server_ip);

	if (connect (client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
		perror("[-] Error connecting");
		close(client_sock);
		return;
	}

	printf("[+] Connected to server %s:%d\n", server_ip, port);
	snprintf(mensaje, sizeof(mensaje), "%d %d %s", port_msg, shift, file_content);

	send(client_sock, mensaje, strlen(mensaje), 0);

	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes > 0) {
		buffer[bytes] = '\0';
		printf("[+] Server Response %d: %s\n", port, buffer);
	} else {
		printf("[-] No response %d\n", port);
	}

	close(client_sock);

}

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Type: %s <Servidor_Ip> <Puerto> <Desplazamiento> <Nombre del archivo>\n", argv[0]);
		return 1;
	}

	char *server_ip = argv[1];
	int puerto = atoi(argv[2]);
	int shift = atoi(argv[3]);
	char *filename = argv[4];

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		perror("[-] Error opening file");
		return 1;
	}

	char file_content[BUFFER_SIZE] = {0};
	fread(file_content, 1, sizeof(file_content)-1, fp);
	fclose(fp);

	conectar_enviar(server_ip, puerto-2, shift, file_content, puerto);
	conectar_enviar(server_ip, puerto-1, shift, file_content, puerto);
	conectar_enviar(server_ip, puerto, shift, file_content, puerto);
	return 0;
}
