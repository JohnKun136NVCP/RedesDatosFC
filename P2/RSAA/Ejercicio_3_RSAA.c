/*
 * Ejercicio 3: Servidor extendido
 * Elaborado por Alejandro Axel Rodríguez Sánchez
 * ahexo@ciencias.unam.mx
 *
 * Facultad de Ciencias UNAM
 * Redes de computadoras
 * Grupo 7006
 * Semestre 2026-1
 * 2025-08-23
 *
 * Profesor:
 * Luis Enrique Serrano Gutiérrez
 * Ayudante de laboratorio:
 * Juan Angeles Hernández
 */

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <time.h>
#include <dirent.h>

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

void saveSystemInfo(FILE *file) {
    struct utsname buffer;
    struct sysinfo sys_info;
    struct statvfs storage_info;
    FILE *fp;
    char command[256];

    uname(&buffer);

    // OS
    fprintf(file, "SO: %s\n", buffer.sysname);

    // Kernel
    fprintf(file, "KERNEL: %s\n", buffer.release);

    // Distribution
    fprintf(file, "DISTRIBUCION: %s\n", buffer.version);

    // Local IP
    fprintf(file, "IP LOCAL: ");
    fp = popen("ip route get 1 | awk '{print $7}'", "r");
    if (fp) {
        fgets(command, sizeof(command), fp);
        fprintf(file, "%s", command);
        pclose(fp);
    }

    // Get CPU information
    fprintf(file, "CPU: ");
    fp = popen("lscpu | grep 'Model name' | cut -f 2 -d ':' | awk '{$1=$1}1'", "r");
    if (fp) {
        fgets(command, sizeof(command), fp);
        fprintf(file, "%s", command);
        pclose(fp);
    }

    // CPU cores
    fprintf(file, "NUCLEOS: ");
    fp = popen("nproc", "r");
    if (fp) {
        fgets(command, sizeof(command), fp);
        fprintf(file, "%s", command);
        pclose(fp);
    }

    // CPU architecture
    fprintf(file, "ARQUITECTURA: %s\n", buffer.machine);

    // RAM
    sysinfo(&sys_info);
    fprintf(file, "MEMORIA: Ocupada: %lu MB, Disponible: %lu MB, Total: %lu MB\n",
            (sys_info.totalram - sys_info.freeram) / (1024 * 1024),
            sys_info.freeram / (1024 * 1024),
            sys_info.totalram / (1024 * 1024));

    // Storage
    statvfs("/", &storage_info);
    fprintf(file, "ALMACENAMIENTO: Ocupado: %lu MB, Disponible: %lu MB, Total: %lu MB\n",
            (storage_info.f_blocks - storage_info.f_bfree) * storage_info.f_frsize / (1024 * 1024),
            storage_info.f_bavail * storage_info.f_frsize / (1024 * 1024),
            storage_info.f_blocks * storage_info.f_frsize / (1024 * 1024));

    // Uptime
    fprintf(file, "UPTIME: ");
    fp = popen("uptime", "r");
    if (fp) {
        fgets(command, sizeof(command), fp);
        fprintf(file, "%s", command);
        pclose(fp);
    }

    // Procesos
    fprintf(file, "PROCESOS ACTIVOS: ");
    fp = popen("ps -e | wc -l", "r");
    if (fp) {
        fgets(command, sizeof(command), fp);
        fprintf(file, "%s", command);
        pclose(fp);
    }

    // Usuarios
    fprintf(file, "USUARIOS:\n");
    struct passwd *pw;
    setpwent();
    while ((pw = getpwent()) != NULL) {
        fprintf(file, "- %s\n", pw->pw_name);
    }
    endpwent();

    // Directorios montados
    fprintf(file, "DIRECTORIOS MONTADOS:\n");
    fp = popen("mount | awk '{print $3}'", "r");
    if (fp) {
        while (fgets(command, sizeof(command), fp) != NULL) {
            fprintf(file, "- %s", command);
        }
        pclose(fp);
    }
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
        FILE *file = fopen("sysinfo.txt", "w");
        saveSystemInfo(file);
        sendFile("sysinfo.txt", client_sock);
        printf("[+] Sent file\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }

    close(client_sock);
    close(server_sock);
    return 0;
}
