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
#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

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

void saveSysInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    // Abrir archivo para guardar la salida
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error to open sysinfo file");
        return;
    }

    // 1. OS y Kernel
    fputs("=== OS y Kernel ===\n", fpOutput);
    fpCommand = popen("uname -a", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 2. Distribución
    fputs("\n=== Distribución ===\n", fpOutput);
    fpCommand = popen("cat /etc/*release", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 3. IPs
    fputs("\n=== IPs ===\n", fpOutput);
    fpCommand = popen("ip -4 addr show | grep inet", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 4. CPU Info
    fputs("\n=== CPU Info ===\n", fpOutput);
    fpCommand = popen("lscpu", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 5. Núcleos
    fputs("\n=== Núcleos ===\n", fpOutput);
    fpCommand = popen("nproc", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 6. Arquitectura
    fputs("\n=== Arquitectura ===\n", fpOutput);
    fpCommand = popen("uname -m", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 7. Memoria
    fputs("\n=== Memoria ===\n", fpOutput);
    fpCommand = popen("free -h", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 8. Disco
    fputs("\n=== Disco ===\n", fpOutput);
    fpCommand = popen("df -h", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 9. Usuarios conectados
    fputs("\n=== Usuarios conectados ===\n", fpOutput);
    fpCommand = popen("who", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 10. Todos los usuarios del sistema
    fputs("\n=== Todos los usuarios del sistema ===\n", fpOutput);
    fpCommand = popen("cut -d: -f1 /etc/passwd", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 11. Uptime
    fputs("\n=== Uptime ===\n", fpOutput);
    fpCommand = popen("uptime -p", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 12. Procesos activos
    fputs("\n=== Procesos activos ===\n", fpOutput);
    fpCommand = popen("ps aux --no-heading | wc -l", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    // 13. Directorios montados
    fputs("\n=== Directorios montados ===\n", fpOutput);
    fpCommand = popen("mount | column -t", "r");
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        fputs(buffer, fpOutput);
    pclose(fpCommand);

    fclose(fpOutput);
}

void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    // Ejecutar comando para obtener información de red
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

    // Leer línea por línea y escribir en el archivo
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

// Función para convertir a minúsculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Función para eliminar espacios al inicio y final
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
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminación
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

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
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

    printf("[+] Server listening port %d...\n", PORT);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }

    printf("[+] Client connected\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }

    buffer[bytes] = '\0';
    sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);

        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1); // Pequeña pausa para evitar colisión de datos

        saveNetworkInfo("network_info.txt");
        sendFile("network_info.txt", client_sock);
        printf("[+] Sent file\n");
        sleep(1);

        saveSysInfo("sysinfo.txt");
        sendFile("sysinfo.txt", client_sock);
        printf("[+] Sent sysinfo file\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }

    close(client_sock);
    close(server_sock);

    return 0;
}
