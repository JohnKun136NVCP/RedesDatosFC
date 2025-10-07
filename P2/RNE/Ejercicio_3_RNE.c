#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#define PORT 7006
#define BUFFER_SIZE 1024

// Función para descifrar un texto usando el cifrado César inverso
void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        }
    }
}

// Función para ejecutar "ip addr show" y guardar la salida en un archivo
void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL) {
        perror("Error ejecutando popen");
        return;
    }

    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error al abrir el archivo de salida");
        pclose(fpCommand);
        return;
    }

    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
    }

    fclose(fpOutput);
    pclose(fpCommand);
}

// Función para obtener información del sistema y guardarla en un archivo
void saveSystemInfo(const char *outputFile) {
    // Abre el archivo en modo escritura. Si existe, lo sobrescribe.
    FILE *fp = fopen(outputFile, "w");
    if (fp == NULL) {
        perror("No se pudo crear el archivo sysinfo.txt");
        return;
    }

    printf("[+] Recopilando informacion del sistema...\n");

    // Lista de comandos a ejecutar con sus descripciones
    const char *commands[][2] = {
        {"OS y Kernel", "uname -a"},
        {"Distribucion", "cat /etc/os-release"},
        {"IPs", "ip a"},
        {"CPU Info", "lscpu"},
        {"Memoria", "free -h"},
        {"Disco", "df -h"},
        {"Usuarios Conectados", "who"},
        {"Uptime", "uptime"},
        {"Procesos Activos", "ps aux"},
        {"Directorios Montados", "mount"}
    };
    int num_commands = sizeof(commands) / sizeof(commands[0]);

    // Itera sobre cada comando, lo ejecuta y guarda su salida
    for (int i = 0; i < num_commands; i++) {
        // Escribe un título para la sección
        fprintf(fp, "\n\n==================== %s ====================\n\n", commands[i][0]);
        
        // Prepara el comando para popen, redirigiendo el error estándar a la salida estándar
        char command_buffer[256];
        snprintf(command_buffer, sizeof(command_buffer), "%s 2>&1", commands[i][1]);

        // Ejecuta el comando usando popen
        FILE *cmd_pipe = popen(command_buffer, "r");
        if (cmd_pipe == NULL) {
            fprintf(fp, "Error al ejecutar el comando: %s\n", commands[i][1]);
            continue;
        }

        // Lee la salida del comando y la escribe en el archivo
        char line[1024];
        while (fgets(line, sizeof(line), cmd_pipe) != NULL) {
            fputs(line, fp);
        }
        pclose(cmd_pipe);
    }
    
    // Cierra el archivo
    fclose(fp);
    printf("[+] Informacion guardada en %s\n", outputFile);
}

// Función para enviar un archivo a través de un socket
void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] No se pudo abrir el archivo para enviar");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error al enviar el archivo");
            break;
        }
    }
    fclose(fp);
}

// Función para convertir una cadena a minúsculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Función para eliminar espacios al inicio y al final de una cadena
void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) start++;

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// Función para verificar si una clave está en el archivo cipherworlds.txt
bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;

    strncpy(buffer, bufferOriginal, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    trim(buffer);
    toLowerCase(buffer);

    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL) {
        printf("[-] Error abriendo cipherworlds.txt!\n");
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

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error al crear el socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Servidor escuchando en el puerto %d...\n", PORT);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error en accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Cliente conectado.\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] No se recibió la clave.\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';
    sscanf(buffer, "%s %d", clave, &shift);
    printf("[+] [Servidor] Clave cifrada obtenida: %s\n", clave);

    if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+] [Servidor] Clave descifrada: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1);
	saveSystemInfo("sysinfo.txt");
    	sendFile("sysinfo.txt", client_sock);
        printf("[+] Archivo enviado.\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-] [Servidor] Clave incorrecta.\n");
    }

    close(client_sock);
    close(server_sock);
    return 0;
}
