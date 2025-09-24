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
#include <poll.h>

#define BUFFER_SIZE 1024 // lenght del buffer para recibir datos
			
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

void saveNetworkInfo(const char *outputFile) {
	FILE *fpCommand;
	FILE *fpOutput;
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

int create_socket(int port) {
	int server_sock;
	struct sockaddr_in server_addr;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

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

int main(int argc, char *argv[]) {
	int client_sock;
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE * 10];
	int ports[] = {49200, 49201, 49202}; //Los puertos a los que se conectara el servidor
	int client = -1;
	// Inicializamos nuestro poll con 6 eventos en este caso, pues son 3 puertos distintos del servidor, es decir
	// 3 sockets de servidor, mientres que hay otros 3 para que se puedan manejar multiples clientes por puerto.
	struct pollfd fds[6];
	// Esto indica el numero de sockets de servidor
	int num_fds = 3;
	
	// Se crean los eventos con la funcion para crear sockets.
	// Se usa POLLIN que quiere decir hay datos que leer.
	for (int i = 0; i < 3; i++) {
		fds[i].fd = create_socket(ports[i]);
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}
	for (int i = 0; i < 3; i++) {
		printf("[*][Client %d] LISTENING...\n", ports[i]);
	};

	int clients[3] = {-1, -1, -1}; //Uso un arreglo de clientes para denotar a los 3 sockets de cliente
	int count_aux = 0;

	while (1) {
		// Definimos nuestro poll() para que espere a realizar la operacion indicada
		// El segundo parametro es el numero de puertos, en este caso sera 3.
		int  eve = poll(fds, num_fds, -1);

		//Ahora se revisa cada socket para cada actividad.
		//es decir, por cada socket de server, se conectara a un socket de cliente
		for (int i = 0; i < num_fds && eve > 0; i++) {
			if (fds[i].revents & POLLIN) {
				eve--;
				
				// Si es menor a 3, quiere decir que es un evento de algun socket de servidor
				if (i < 3) {
					// Este if es para inicializar y conectar a un nuevo cliente si es que hay una conexion
					// para enlazarlo con un socket de servidor.
					if (clients[i] == -1) {
						addr_size = sizeof(client_addr);
						int new_client = accept(fds[i].fd, (struct sockaddr *)&client_addr,&addr_size);
						// Esto se podria manejar mejor, pues en mi codigo no hay forma de que falle
						// por lo que no considere algo muy complicado en caso de que falle
						if (new_client < 0) {
							perror("[-] Error on accept");
							close(fds[i].fd);
							continue;

						}
						clients[i] = new_client; // actualizamos nuestro arreglo de enteros con el socket de cliente creado
						// Agregamos al socket del cliente como un nuevo evento en nuestro arreglo de eventos, dado que num_fds
						// lleva el rastreo de que eventos ya estan, este inicia en 3, luego en 4 y por ultimo en 5 en nuestra ejecucion
						fds[num_fds].fd = new_client;
						fds[num_fds].events = POLLIN;
						fds[num_fds].revents = 0;
						num_fds++;
					}
				} else {
					// Esto solo sucede cuando se maneja la conexion del cliente
					// Podemos obtener el socket del cliente puest i >= 3, es decir, es el rango en el arreglo de eventos para los
					// sockets de clientes.
					int client_sock = fds[i].fd;
					int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
					// Esto de igual manera se podria manejar mejor
					if (bytes <= 0) {
						printf("[-] Missed Arguments\n");
						close(client_sock);
						client_sock = -1;
						num_fds--;
						return 1;
					}
					buffer[bytes] = '\0';
					
					// Lo siguiente se hace para poder separar el mensaje enviado por el cliente
					// dado que el mensaje esta divido por | segun el contenido
					char *last_pipe = strrchr(buffer, '|');

					int shift = atoi(last_pipe + 1);

					*last_pipe = '\0'; 
					char *second_pipe = strrchr(buffer, '|');

					int socket_port = atoi(second_pipe + 1);
					*second_pipe = '\0'; 

					char *file_content = (char*)buffer;

					// Esto es para obtener el indice en el arreglo de sockets de cliente
					// para posteriormente obtener el puerto y poder cerrar el socket
					int port_index = -1;
					for (int j = 0; j < 3; j++) {
						if (clients[j] == client_sock) {
							port_index = j;
							break;
						}	
					}

					// Dado que nuestros arreglos son del mismo length, puedo hacer esto para obtener
					// el puerto que el cliente esta solicitando, pues en mi cliente los sockets se crean
					// de manera secuencial, es decir, primero 49200, luego 49201 y por ultimo 49202.
					int actual_port = ports[port_index];

					if (socket_port == actual_port) {
						encryptCaesar(file_content, shift);
						printf("[*][Server %d] File received and encrypted like file_%d_cesar.txt\n", socket_port, socket_port);
						saveEncryptedText(socket_port, file_content);
						send(client_sock, "File received and encrypted.", strlen("File received and encrypted."), 0);
					} else {
						send(client_sock, "REJECTED", strlen("REJECTED"), 0);
						fprintf(stderr,"[*] [SERVER %d] Request rejected (client requested port %d)\n", actual_port, socket_port);
					}

					// Podemos cerrar el socket del cliente
					clients[port_index] = -1;
					close(client_sock);
					
					// Dado que termina la transmision de datos de un socket del cliente, ajustamos el arreglo de eventos
					// para que siga de forma consecutiva, es decir, recorremos a la izquierda.
					for (int k = i; k < num_fds - 1; k++) {
						fds[k] = fds[k+1];
					}
					// Dado que cerramos una conexion, disminuimos nuestro numero de eventos
					num_fds--;
					// Tambien ajustamos nuestra i pues cerramos una conexion
					i--;
					count_aux++;
					// Esto solo se usa para acabar el programa una vez se manejan las tres conexiones
					if (count_aux >= 3) {
						for (int i = 0; i < 3; i++) {
							close(fds[i].fd);
							return 0;
						}
					}
				}
			}
		}
	}
}
