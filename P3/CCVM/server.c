#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006 // Puerto en el que el servidor escucha
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

// En lugar de restar el shift, solo sumamos para encriptar
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

// Ejercicio 3
// Función que imprime la información del servidor
void saveServerInfo() {
	// El nombre del archivo en el que se guardará o escribirá la información
	char outputFile[] = "sysinfo.txt";
	FILE *fpCommand;
	FILE *fpOutput;
	// Se declara un buffer en el que se almacenara la información de los
	// comandos, se usa el máximo solo para asegurar de que no haya
	// buffer overflow
	char buffer[8192];

	char *commands[] = {
	    "lsb_release -a", "Distribución info",
	    "uname -a", "Kernel info",
	    "ip addr show", "IPs info",
	    "lscpu", "CPU info",
	    "nproc", "Número de núcleos",
	    "uname -m", "Arquitectura",
	    "free -h", "Memoria RAM info",
	    "df -h", "Memoria Info",
	    "lsblk", "Disco Info",
	    "who", "Usuarios Conectados",
	    "cat /etc/passwd", "Todos los Usuarios del Sistema",
	    "uptime", "Uptime",
	    "ps aux", "Procesos Activos",
	    "mount", "Directorios Montados"
	};

	// Abrir archivo para guardar la salida
	fpOutput = fopen(outputFile, "w");
	if (fpOutput == NULL) {
		perror("[-] Error to open the file");
		return;
	}
	
	// Para obtener el número total de elementos del arreglo de comandos
	int total_elements = sizeof(commands) / sizeof(commands[0]);

	for (int i = 0; i < total_elements; i += 2) {
	    // Comando que usará para obtener la información del servidor
	    // Se usa la opción r para que sea solo de lectura
	    fpCommand = popen(commands[i], "r");
	    if (fpCommand == NULL) {
		    perror("Error!");
		    return;
	    }
	    
	    // Para escribir en el archivo el tipo de información que se escribirá
	    fprintf(fpOutput, "\n ---------- %s ---------- \n", commands[i+1]);

	    // Leer linea por linea y escribir en el resultado concreto
	    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
		fprintf(fpOutput, "%s", buffer);
	    }
	    
	    // Cerrar el archivo del comand pues se escribirá otro distinto
	    pclose(fpCommand);
	}

	// Cerrar ambos archivos
	fclose(fpOutput);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Type %s <PORT> \n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE * 10];
    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[*] [Client %d] LISTENING...\n", port);
    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);

    if (client_sock < 0) {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed Arguments\n");
        close(client_sock);
        close(server_sock);
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

    if (socket_port == port) {
        encryptCaesar(file_content, shift);
        printf("[*] [Server %d] Request accepted... \n", socket_port);
        printf("[*] [Server %d] File received and encrypted: %s\n", socket_port, file_content);
        send(client_sock, "File received and encrypted.", strlen("File received and encrypted."), 0);
    } else {
        send(client_sock, "REJECTED", strlen("REJECTED"), 0);
        fprintf(stderr, "[*] [SERVER %d] Request rejected (client requested port %d)\n", socket_port, port);
        close(server_sock);
        return 1;
    }

    close(client_sock);

    return 0;
}
