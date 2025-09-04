#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#define PORT 7006
#define BUFFER_SIZE 1024

void decryptCaesar(char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}


void saveNetworkInfo(const char *outputFile){
	FILE *fpCommand;
	FILE *fpOutput;
	char buffer[512];
	
	fpCommand = popen("ip addr show", "r");
	if (fpCommand == NULL) {
		perror("Error!");
		return;
	}
	
	fpOutput = fopen(outputFile, "w");
	if (fpOutput == NULL) {
		perror("[-] Error to open the file");
		pclose(fpCommand);
		return;
	}
	
	while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
		fputs(buffer, fpOutput);
	}
	
	fclose(fpOutput);
	pclose(fpCommand);
}

void sendFile(const char *filename, int sockfd) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
	perror("[-] Cannot open the file");
	return;
	}
	char buffer[BUFFER_SIZE];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
	if (send(sockfd, buffer, bytes, 0) == -1) {
	perror("[-] Error to send the file");
	break;
	}
	}
	fclose(fp);
}


void toLowerCase(char *str) {
for (int i = 0; str[i]; i++)
str[i] = tolower((unsigned char)str[i]);
}


void trim(char *str) {
	char *end;
	while (isspace((unsigned char)*str)) str++; 
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;
	*(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal) {
	FILE *fp;
	char line[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
	bool foundWorld = false;
	strncpy(buffer, bufferOriginal, BUFFER_SIZE);
	buffer[BUFFER_SIZE - 1] = '\0'; 
	trim(buffer);
	toLowerCase(buffer);
	fp = fopen("cipherworlds.txt", "r");
	if (fp == NULL) {
		printf("[-]No se pudo abrir el archivo!\n");
		return false;
	}else{
		printf("[+] Archivo abierto (:\n");
	}
	while (fgets(line, sizeof(line), fp) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		trim(line);
		toLowerCase(line);
		if (strcmp(line, buffer) == 0) {
			foundWorld = true;
			break;
		}else{
			printf("[-] No se encontrÃ³ la palabra %s\n", line);
		}
	}
	fclose(fp);
	return foundWorld;
}

int main() {

	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0};
	char clave[BUFFER_SIZE];
	int shift;
	
	
	server_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (server_sock == -1) {
		perror("[-] No se pudo crear el socket");
		return 1;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;


	if (bind(server_sock, (struct sockaddr *) & server_addr,sizeof(server_addr))< 0) {
		perror("[-] Error binding");
		close(server_sock);
		return 1;
	}


	if (listen(server_sock, 1) < 0) {
		perror("[-] No pudo hacer listen");
		close(server_sock);
		return 1;
	}

	printf("[+] Server listening port %d...\n", PORT);

	addr_size = sizeof(client_addr);
	client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
	

	if (client_sock < 0) {
		perror("[-] No pudo aceptar");
		close(server_sock);
		return 1;
	}

	printf("[+] COnexiÃ³n exitosa\n");

	// Recibe la clave sifrada y el desplazamiento
	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	

	if (bytes <= 0) {
		printf("[-] Llave perdida\n");
		close(client_sock);
		close(server_sock);
		return 1;
	}

	buffer[bytes] = '\0';
	sscanf(buffer, "%s %d", clave, &shift);

	printf("[+][Server] Llave obtenida: %s\n", clave);

 
	if (isOnFile(clave)) {
		decryptCaesar(clave, shift);
		printf("[+][Server] Key decrypted: %s\n", clave);
		send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
		sleep(1); 
		saveNetworkInfo("network_info.txt");
		sendFile("network_info.txt", client_sock);
		printf("[+] Sent file\n");
	} else {
		send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
		printf("[-][Server] Wrong Key\n");
	}


	close(client_sock);
	close(server_sock);
	return 0;
}
