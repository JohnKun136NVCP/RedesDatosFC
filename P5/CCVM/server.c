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
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024 // length del buffer para recibir datos
#define MAIN_PORT 49200 // el port en el que escuchara el servidor
#define SLEEP_TIME 10000 // el tiempo para dormir de cada servidor

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

// Modificado para poder guardar dependiendo del alias pasado
void saveFile(char *text, char *file_name, const char *base_dir) {
	FILE *fpOutput;
	char filename[8192];
	snprintf(filename, sizeof(filename), "%s/%s", base_dir, file_name);
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

	if (listen(server_sock, 10) < 0) {
		perror("[-] Error on listen");
		close(server_sock);
		return 1;
	}

	// Coloca el socket en el modo non-blocking, esto para poder ser usado
	// en la calendarizacion Round Robin
	if (fcntl(server_sock, F_SETFL, fcntl(server_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
		perror("[-] Error setting the socket to non-blocking");
		close(server_sock);
		return -1;
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
void handle_client(int client_sock, const char *server_ip, const char *alias) {
	char buffer[BUFFER_SIZE * 10];
	struct sockaddr_in claddr, local_addr;
	socklen_t claddr_len = sizeof(claddr);
	socklen_t local_len = sizeof(local_addr);
	
	// Esto se pasa a esta funcion para poder almacenar el archivo recibido
	// en el directorio asignado.
	char dir_path[512];
	char *home_dir = getenv("HOME");
	if (!home_dir) {
		fprintf(stderr, "[-] Error: HOME environment variable not found.\n");
		close(client_sock);
		exit(1);
	}
	snprintf(dir_path, sizeof(dir_path), "%s/%s", home_dir, alias);

	int flags = fcntl(client_sock, F_GETFL, 0);
	fcntl(client_sock, flags & (~O_NONBLOCK));
	
	getsockname(client_sock, (struct sockaddr *)&local_addr, &local_len);
	char connected_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &local_addr.sin_addr, connected_ip, INET_ADDRSTRLEN);
	int new_port = ntohs(local_addr.sin_port);

	printf("[*][Server connected on port %d via IP %s] Handling request.\n", new_port, connected_ip);
	
	
	if (strcmp(connected_ip, server_ip) != 0) {
		fprintf(stderr, "[-] Connection rejected: IP mismatch. \n");
		close(client_sock);
		return;
	}

	printf("[*][Server connected on port %d] Waiting for client response.\n", new_port);
	
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

	if (socket_port == new_port) {
		saveFile(file_content, filename, dir_path);
		fprintf(stderr,"[*][Server connected on port %d] Saved %s on the directory %s\n", socket_port, filename, dir_path);
		send(client_sock, "Status: transmitting data", strlen("Status: transmitting data"), 0);
	} else {
		send(client_sock, "REJECTED", strlen("REJECTED"), 0);
		fprintf(stderr,"[*][Server %d] Request rejected\n", new_port);
	}

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
		printf("Type: %s <alias1> <alias2> ... <aliasN> \n", argv[0]);
		return 1;
	}
	
	int num_aliases = argc - 1;
	char *alias_names[num_aliases];
	char *alias_ips[num_aliases];
	int listen_socks[num_aliases];
	int next_port = MAIN_PORT;
	
	// Ciclo para definir todos los sockets para cada ip, asignando nuevos
	// port arriba de 49200 para poder manejar multiples conexiones
	// de forma que se puedan calendarizar
	for (int i = 0; i < num_aliases; i++) {
		char *alias = argv[i+1];
		char *ip_address = get_ip(alias);
		alias_names[i] = alias;
		if (ip_address == NULL) continue;
		
		alias_ips[i] = strdup(ip_address);
		int sock = create_socket(next_port, ip_address);
		if (sock < 0) continue;
		
		listen_socks[i] = sock;
		printf("[*] Server with alias %s and IP %s on port %d is listening...\n", alias, ip_address, next_port);
		next_port++;	
	}

	int index = 0; // El indice de la implementacion de Round Robin

	while (1) {
		int current_sock = listen_socks[index];
		char *current_alias = alias_names[index];	
		char *current_ip = alias_ips[index];
		
		struct sockaddr_in client_addr;
		socklen_t addr_size = sizeof(client_addr);
		
		int client_sock = accept(current_sock, (struct sockaddr *)&client_addr, &addr_size);
		if (client_sock < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {	
				// Si ocurre este caso, quiere decir que no hay conexion
				// que hacer, por lo que no hacemos nada
			} else {
				perror("[-] error on accept");
			}
		} else {
			// Ocurrió una conexión
			pid_t pid = fork();
			if (pid == 0) {
				// Es un proceso hijo, por lo que cerramos
				// todos los sockets de escucha 
				// y maneja el cliente
				for (int i = 0; i < num_aliases; i++)
					close(listen_socks[i]);
				handle_client(client_sock, current_ip, current_alias);
				exit(0);
			} else {
				// El proceso es el proceso padre, por lo que no hace falta el
				// socket del cliente, solo el de escucha.
				close(client_sock);
				// Espera a que cualquier proceso hijo termine.
				waitpid(-1, NULL, WNOHANG);
			}
		}
		// Nos movemos al siguiente al que le toca en la calendarizacion
		// round robin, haciendo el modulo para que no se salga
		// del tamaño del arreglo
		index = (index + 1) % num_aliases;
		// Aplicamos el quantum, es decir, dormimos al proceso un tiempo determinado
		// En este caso siendo 10 milisegundos
		usleep(SLEEP_TIME);  
	}
	return 0;
}
