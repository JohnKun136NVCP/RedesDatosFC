#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1024

void logStatus(const char *server_ip, int port, const char *status) {
    FILE *log = fopen("client_log.txt", "a");
    if (log) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strcspn(ts, "\n")] = 0;
        fprintf(log, "[%s] Servidor %s:%d Estado: %s\n", ts, server_ip, port, status);
        fclose(log);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <SERVER_IP> <BASE_PORT> <FILE>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int base_port = atoi(argv[2]);
    const char *filename = argv[3];

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Conexión al puerto base para recibir puerto dinámico
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(base_port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        logStatus(server_ip, base_port, "Fallo conexión al puerto base");
        close(sock);
        return 1;
    }

    int recv_bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (recv_bytes <= 0) { perror("recv"); close(sock); return 1; }
    buffer[recv_bytes] = '\0';
    int dyn_port = atoi(buffer);
    printf("[+] Puerto dinámico asignado por servidor: %d\n", dyn_port);
    close(sock);

    // Conectar al puerto dinámico
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    serv_addr.sin_port = htons(dyn_port);
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        logStatus(server_ip, dyn_port, "Fallo conexión al puerto dinámico");
        close(sock);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror("fopen"); close(sock); return 1; }

    int bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(sock, buffer, bytes, 0);
        int echo_bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (echo_bytes > 0) {
            buffer[echo_bytes] = '\0';
            printf("[Echo from server]: %s\n", buffer);
        }
    }
    fclose(fp);
    logStatus(server_ip, dyn_port, "Archivo enviado y eco recibido");
    close(sock);
    return 0;
}
