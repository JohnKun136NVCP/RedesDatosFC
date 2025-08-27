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

void saveSystemInfo(const char *outputFile) {
    FILE *fp = fopen(outputFile, "w");
    if (fp == NULL) {
        perror("[-] Error al abrir sysinfo.txt");
        return;
    }

    // Ejecutar comandos y guardar salida
    fprintf(fp, "=== INFORMACIÓN DEL SISTEMA ===\n\n");

    // 1. OS y Kernel
    fprintf(fp, "1. OS y Kernel:\n");
    system("uname -a >> sysinfo.txt");
    fprintf(fp, "\n");

    // 2. Distribución
    fprintf(fp, "2. Distribución:\n");
    system("lsb_release -a 2>/dev/null >> sysinfo.txt || echo 'No disponible' >> sysinfo.txt");
    fprintf(fp, "\n");

    // 3. IPs
    fprintf(fp, "3. Direcciones IP:\n");
    system("ip addr show | grep 'inet ' >> sysinfo.txt");
    fprintf(fp, "\n");

    // 4. CPU Info
    fprintf(fp, "4. Información de CPU:\n");
    system("lscpu | grep -E 'Model name|Socket|Core|CPU(s)' >> sysinfo.txt");
    fprintf(fp, "\n");

    // 5. Arquitectura
    fprintf(fp, "5. Arquitectura:\n");
    system("uname -m >> sysinfo.txt");
    fprintf(fp, "\n");

    // 6. Memoria
    fprintf(fp, "6. Memoria:\n");
    system("free -h >> sysinfo.txt");
    fprintf(fp, "\n");

    // 7. Disco
    fprintf(fp, "7. Uso de disco:\n");
    system("df -h >> sysinfo.txt");
    fprintf(fp, "\n");

    // 8. Usuarios conectados
    fprintf(fp, "8. Usuarios conectados:\n");
    system("w >> sysinfo.txt");
    fprintf(fp, "\n");

    // 9. Todos los usuarios del sistema
    fprintf(fp, "9. Todos los usuarios:\n");

    system("cut -d: -f1 /etc/passwd >> sysinfo.txt");
    fprintf(fp, "\n");

    // 10. Uptime
    fprintf(fp, "10. Tiempo de actividad:\n");
    system("uptime >> sysinfo.txt");
    fprintf(fp, "\n");

    // 11. Procesos activos
    fprintf(fp, "11. Procesos activos (top 5):\n");
    system("ps aux --sort=-%cpu | head -6 >> sysinfo.txt");
    fprintf(fp, "\n");

    // 12. Directorios montados
    fprintf(fp, "12. Directorios montados:\n");
    system("mount >> sysinfo.txt");

    fclose(fp);
}



void saveNetworkInfo(const char *outputFile) {
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
    sscanf(buffer, "%s %d", clave, &shift);
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1);
        //saveNetworkInfo("network_info.txt");
        saveSystemInfo("sysinfo.txt");
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