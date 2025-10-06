#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

void get_current_time(char *time_str) {
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    sprintf(time_str, "%02d/%02d/%d %02d:%02d:%02d",
            local->tm_mday, local->tm_mon + 1, local->tm_year + 1900,
            local->tm_hour, local->tm_min, local->tm_sec);
}

// Imprimimos y guardamos en log el dia y la hora de cada evento, asi como la IP y el puerto del servidor, el nombre del archivo y el estado
void log_event(const char *server_ip, int port, const char *filename, const char *status) {
    char time_str[64];
    get_current_time(time_str);

    FILE *fp = fopen("client_activity.log", "a");
    if (fp != NULL) {
        fprintf(fp, "[%s] Server: %s:%d | File: %s | Status: %s\n",
                time_str, server_ip, port, filename, status);
        fclose(fp);
    }

    printf("[%s] Logged: %s:%d - %s - %s\n", time_str, server_ip, port, filename, status);
}

void applyCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

// Función para leer archivo completo
char* readFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Cannot open file");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(fp);
        return NULL;
    }

    fread(content, 1, file_size, fp);
    content[file_size] = '\0';

    // Eliminar saltos de línea finales para el cifrado
    while (file_size > 0 && (content[file_size-1] == '\n' || content[file_size-1] == '\r')) {
        content[--file_size] = '\0';
    }

    fclose(fp);
    return content;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <server_ip> <port> <shift> <file_path>\n", argv[0]);
        printf("Example: %s 192.168.1.103 49200 5 sample1.txt\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *file_path = argv[4];

    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE * 2];
    char *filename = strrchr(file_path, '/');
    if (filename == NULL) {
        filename = file_path;
    } else {
        filename++;
    }

    // Log de intento de conexión
    log_event(server_ip, port, filename, "ATTEMPTING CONNECTION");

    // Leer el contenido del archivo
    char *file_content = readFile(file_path);
    if (file_content == NULL) {
        printf("[-] Error reading file: %s\n", file_path);
        log_event(server_ip, port, filename, "FILE READ ERROR");
        return 1;
    }

    // Aplicar cifrado César al contenido
    applyCaesar(file_content, shift);

    printf("[+] File content encrypted with shift %d\n", shift);

    // Crear socket
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error creating socket");
        log_event(server_ip, port, filename, "SOCKET ERROR");
        free(file_content);
        return 1;
    }

    // Configurar servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Conectar
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Could not connect to server");
        log_event(server_ip, port, filename, "CONNECTION FAILED - SERVER OFFLINE");
        close(client_sock);
        free(file_content);
        return 1;
    }

    printf("[+] Connected to %s:%d\n", server_ip, port);
    log_event(server_ip, port, filename, "CONNECTED - SERVER ONLINE");

    // Preparar mensaje en formato: "contenido_cifrado|shift"
    snprintf(message, sizeof(message), "%s|%d", file_content, shift);

    // Enviar mensaje
    send(client_sock, message, strlen(message), 0);
    printf("[+] File sent with shift %d\n", shift);

    // Esperar respuesta del servidor
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+] Server response: %s\n", buffer);
    } else {
        printf("[-] No response from server\n");
        log_event(server_ip, port, filename, "NO SERVER RESPONSE");
    }

    close(client_sock);
    free(file_content);

    printf("[+] File %s processed\n", filename);
    return 0;
}