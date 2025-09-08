#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

void sendFileToPort(const char *server_ip, int port, const char *file, int shift) {
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) { perror("socket"); return; }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); close(client_sock); return;
    }

    printf("[+] Connected to %s:%d\n", server_ip, port);

    // Enviar nombre del archivo + shift como clave
    snprintf(mensaje, sizeof(mensaje), "%s %d", file, shift);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Sent: %s\n", mensaje);

    // Recibir respuesta del servidor
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) { printf("[-] No response from server %d\n", port); close(client_sock); return; }
    buffer[bytes] = '\0';
    printf("[+][Client] Server %d says: %s\n", port, buffer);

    // Si ACCESS GRANTED, recibir archivos
    if (strstr(buffer, "ACCESS GRANTED") != NULL) {
        char out_filename[BUFFER_SIZE];
        snprintf(out_filename, sizeof(out_filename), "received_%s_port%d.txt", file, port);
        FILE *fp = fopen(out_filename, "wb");
        if (!fp) { perror("fopen"); close(client_sock); return; }

        while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes, fp);
        }
        fclose(fp);
        printf("[+] Received file saved as %s\n", out_filename);
    }

    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        printf("Usage: %s <IP> <PORT1> <PORT2> <PORT3> <FILE1> <FILE2> <FILE3> <SHIFT>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int ports[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])};
    const char *files[3] = {argv[5], argv[6], argv[7]};
    int shift = atoi(argv[8]);

    for (int i = 0; i < 3; i++) {
        sendFileToPort(server_ip, ports[i], files[i], shift);
    }

    return 0;
}
