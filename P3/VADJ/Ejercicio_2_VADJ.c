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
	if (argc != 2) {
		printf("Type: %s <port>\n", argv[0]);
		return 1;
	}

	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0};
	char clave[BUFFER_SIZE];
	int shift;
	int port = atoi(argv[1]);
	int client_port;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);


	if (server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind (server_sock, (struct sockaddr *)&server_addr, sizeof (server_addr)) < 0) {
		perror("[-] Error binding");
		close(server_sock);
		return 1;
	}

	if (listen (server_sock, 1) < 0 ) {
		perror("[-] Error on listen");
		close(server_sock);
		return 1;
	}

	printf("[+] Sever listening port %d...\n", port);
	addr_size = sizeof(client_addr);
	client_sock = accept (server_sock, (struct sockaddr *)&client_addr, &addr_size);

	if (client_sock < 0 ) {
		perror("[-] Error on accept");
		close(server_sock);
		return 1;
	}

	printf("[+] Client connected\n");
	int bytes = recv (client_sock, buffer, sizeof(buffer) - 1, 0);

	if (bytes <= 0) {
		printf("[-] Missed key\n");
		close(client_sock);
		close(server_sock);
		return 1;
	}

	buffer[bytes] = '\0';
	sscanf (buffer, "%d %d %[^\n]", &client_port, &shift, clave);
	if (client_port != port) {
		fprintf(stderr, "[-] [SERVER] Request rejected (client requested port: %d)", client_port);
		send(client_sock, "REJECTED", strlen("REJECTED"), 0);
		return 1;
	}
	printf("[+] [Server] Encrypted key obtained: %s\n", clave);
	cryptCaesar(clave, shift);
	printf("[+] [Server] File received and encrypted:\n %s\n", clave);
	send (client_sock, "File received and encrypted", strlen("File received and encrypted"), 0);
	sleep(1);

	close (client_sock);
	close (server_sock);
	return 0;
}
