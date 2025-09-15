#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <netdb.h>

#define BUFFER_SIZE 1024 // length del buffer para recibir datos
#define MAIN_PORT 49200 // el port en el que escuchara el servidor

char dir_path[256]; // El directorio asignado dado el alias al servidor.
			
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

void encryptCaesar(char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' + shift) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' + shift) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}

void saveEncryptedText(int port, char *text) {
	FILE *fpOutput;
	char filename[8192];
	sprintf(filename, "file_%d_cesar.txt", port);
	fpOutput = fopen(filename, "w");
	if (fpOutput == NULL) {
		perror("[-] Error to open the file");
		return;
	}

	fprintf(fpOutput, "%s", text);

	fclose(fpOutput);
}

void saveFile(char *text, char *file_name) {
	FILE *fpOutput;
	char filename[8192];
	snprintf(filename, sizeof(filename), "%s/%s", dir_path, file_name);
	fpOutput = fopen(filename, "w");
	if (fpOutput == NULL) {
		perror("[-] Error to open the file");
		return;
	}

	fprintf(fpOutput, "%s", text);

	fclose(fpOutput);
}

void saveNetworkInfo(const char *outputFile) {
	FILE *fpCommand; FILE *fpOutput;
	char buffer[512];

	// Ejecutar comando para obtener information de red
	fpCommand = popen("ip addr show", "r");
	if (fpCommand == NULL) {
		perror("Error!");
		return;
	}

	// Abrir archivo para guardar la salida
	fpOutput = fopen(outputFile, "w");
	if (fpOutput == NULL) {
		perror("[-] Error to open the file");
		pclose(fpCommand);
		return;
	}

	// Leer linea por linea y escribir en el archivo
	while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
		fputs(buffer, fpOutput);
	}

	// Cerrar ambos archivos
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

// Function para convertir a minusculas
void toLowerCase(char *str) {
	for (int i = 0; str[i]; i++)
		str[i] = tolower((unsigned char)str[i]);
}

void trim(char *str) {
	char *end;
	while (isspace((unsigned char)*str)) str++; // inicio
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--; // final
	*(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal) {
	FILE *fp;
	char line[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
	bool foundWorld = false;

	// Copiamos el buffer original para poder modificarlo
	strncpy(buffer, bufferOriginal, BUFFER_SIZE);
	buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminacion
	trim(buffer);
	toLowerCase(buffer);

	fp = fopen("cipherworlds.txt", "r");
	if (fp == NULL) {
		printf("[-]Error opening file!\n");
		return false;
	}
	while (fgets(line, sizeof(line), fp) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		trim(line);
		toLowerCase(line);
		if (strcmp(line, buffer) == 0) {
			foundWorld = true;
			break;
		}
	}
	fclose(fp);
	return foundWorld;
}

int create_socket(int port, const char* ip_addr) {
	int server_sock;
	struct sockaddr_in server_addr;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
		fprintf(stderr, "[-] Error: Invalid IP address: %s\n", ip_addr);
		close(server_sock);
		return -1;
	} 

	int opt = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))
			< 0) {
		perror("[-] Error binding");
		close(server_sock);
		return 1;
	}

	if (listen(server_sock, 1) < 0) {
		perror("[-] Error on listen");
		close(server_sock);
		return 1;
	}
	
	return server_sock;
}

void remove_client(int client_sock, int clients[]) {
	for (int i = 0; i < 3; i++) {
		if (clients[i] == client_sock) {
			clients[i] = -1;
			break;
		}
	}
}

// Funcion para hacer mas legible el main 
// y manejar de mejor manera las conexiones
// La conexion se cierra aqui pues se ejecutara en un proceso hijo
// usando fork
void handle_client(int client_sock, const char *server_ip) {
	char buffer[BUFFER_SIZE * 10];
	struct sockaddr_in claddr, local_addr;
	socklen_t claddr_len = sizeof(claddr);
	socklen_t local_len = sizeof(local_addr);
	
	getsockname(client_sock, (struct sockaddr *)&local_addr, &local_len);
	char connected_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &local_addr.sin_addr, connected_ip, INET_ADDRSTRLEN);
	
	if (strcmp(connected_ip, server_ip) != 0) {
		fprintf(stderr, "[-] Connection rejected: IP mismatch. \n");
		close(client_sock);
		return;
	}

	// Obtiene la informacion del cliente, es decir su direccion ip
	// y el port dinamico asignado
	getpeername(client_sock, (struct sockaddr *)&claddr, &claddr_len);
	int port = ntohs(claddr.sin_port);

	printf("[*][Server connected on port %d] Waiting for client response.\n", port);
	
	// La conexión ya se realizó, por lo que se manda un update de esperando
	// por un archivo
	send(client_sock, "Status: waiting for file", strlen("Status: waiting for file"), 0);

	send(client_sock, "Status: receiving file", strlen("Status: receiving file"), 0);

	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		printf("[-] Missed Arguments or connection closed.\n");
		close(client_sock);
		exit(1); }
	buffer[bytes] = '\0';
	
	// Lo siguiente se hace para poder separar el mensaje enviado por el cliente
	// dado que el mensaje esta divido por | segun el contenido
	char *filename = strrchr(buffer, '|');
	*filename = '\0'; 
	filename++;

	char *second_pipe = strrchr(buffer, '|');

	int socket_port = atoi(second_pipe + 1);
	*second_pipe = '\0'; 

	char *file_content = (char*)buffer;

	if (socket_port == MAIN_PORT) {
		saveFile(file_content, filename);
		fprintf(stderr,"[*][Server connected on port %d] Saved %s on the directory %s\n", port, filename, dir_path);
		send(client_sock, "Status: transmitting data", strlen("Status: transmitting data"), 0);
	} else {
		send(client_sock, "REJECTED", strlen("REJECTED"), 0);
		fprintf(stderr,"[*][Server %d] Request rejected\n", port);
	}
	printf("[*][Server on port %d] listening...\n", MAIN_PORT);

	close(client_sock);
	exit(0);
}

// Función para obtener la dirección ip dado un alias
char* get_ip(const char* alias) {
	struct addrinfo hints, *result;
	static char ip_str[INET_ADDRSTRLEN];
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	int status = getaddrinfo(alias, NULL, &hints, &result);
	if (status != 0) {
		fprintf(stderr, "[-] Error: alias not resolved %s: %s\n", alias, gai_strerror(status));
		return NULL;
	}
	struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
	inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
	freeaddrinfo(result);
	return ip_str;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Type: %s <server_alias>\n", argv[0]);
		return 1;
	}
	
	char *alias = argv[1];

	char *ip_address = get_ip(alias);
	if (ip_address == NULL) {
		return 1;
	}

	int listen_sock = create_socket(MAIN_PORT, ip_address);
	if (listen_sock < 0) {
		return 1;
	}	

	char *home_directory = getenv("HOME");
	if (home_directory == NULL) {
		fprintf(stderr, "[-] Error: HOME environment variable not found.\n");
		return 1;
	}
	snprintf(dir_path, sizeof(dir_path), "%s/%s", home_directory, alias);
	printf("[!] The home directory of the server is: %s\n", dir_path); 

	while (1) {
		printf("[*][Server on port %d] listening...\n", MAIN_PORT);
		struct sockaddr_in client_addr;
		socklen_t addr_size = sizeof(client_addr);
		
		int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_size);
		if (client_sock < 0) {
			perror("[-] error on accept");
			continue;
		}

		pid_t pid = fork();
		if (pid == 0) {
			// Este es un proceso hijo, por lo que no es necesario
			// que este escuchando con el socket principal
			// es decir el que tiene puerto 49200
			close(listen_sock);
			handle_client(client_sock, ip_address);
		} else {
			// El proceso es el proceso padre, por lo que no hace falta el
			// socket del cliente, solo el de escucha.
			close(client_sock);
			// Espera a que cualquier proceso hijo termine.
			waitpid(-1, NULL, WNOHANG);
		}
	}
	return 0;
}
