#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define PORT 49200
#define BUFFER_SIZE 1024

void save_file(int sockfd, const char *folder) {
    char buffer[BUFFER_SIZE];
    int bytes;
    
    // Obtener nombre único para el archivo recibido
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/file_%04d%02d%02d_%02d%02d%02d.txt",
             folder,
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("[-] Cannot open file to save");
        return;
    }

    // Recibir datos del cliente
    while ((bytes = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
        if (bytes < BUFFER_SIZE) break; // fin de archivo
    }

    fclose(fp);

    // Guardar log
    FILE *logfp = fopen("server_log.txt", "a");
    if (logfp) {
        fprintf(logfp, "[%04d-%02d-%02d %02d:%02d:%02d] File received: %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                filename);
        fclose(logfp);
    }

    printf("[+] File saved: %s\n", filename);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <alias_folder1> [alias_folder2 ...]\n", argv[0]);
        return 1;
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-] Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Bind failed");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("[-] Listen failed");
        close(server_sock);
        exit(1);
    }

    printf("[+] Server listening on port %d\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] Accept failed");
            continue;
        }

        printf("[+] Client connected\n");

        // Para simplificar, recibir carpeta del cliente (alias)
        char alias_folder[64];
        int r = recv(client_sock, alias_folder, sizeof(alias_folder) - 1, 0);
        if (r <= 0) {
            printf("[-] Failed to receive alias\n");
            close(client_sock);
            continue;
        }
        alias_folder[r] = '\0';

        // Construir path completo
        char folder_path[256];
        snprintf(folder_path, sizeof(folder_path), "%s/%s", getenv("HOME"), alias_folder);

        // Crear carpeta si no existe
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", folder_path);
        system(cmd);

        // Guardar archivo recibido
        save_file(client_sock, folder_path);

        close(client_sock);
    }

    close(server_sock);
    return 0;
}
