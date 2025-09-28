#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1024

void logStatus(const char *server_ip, int port, const char *status, const char *filename) {
    FILE *log = fopen("client_log.txt", "a");
    if (log) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strcspn(ts, "\n")] = 0;
        if (filename)
            fprintf(log, "[%s] Servidor %s:%d Archivo: %s Estado: %s\n", ts, server_ip, port, filename, status);
        else
            fprintf(log, "[%s] Servidor %s:%d Estado: %s\n", ts, server_ip, port, status);
        fclose(log);
    }
}

int connect_dynamic_port(const char *server_ip, int base_port) {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Conexión al puerto base
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(base_port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        logStatus(server_ip, base_port, "Fallo conexión al puerto base", NULL);
        close(sock);
        return -1;
    }

    // Recibir puerto dinámico
    int recv_bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (recv_bytes <= 0) { perror("recv"); close(sock); return -1; }
    buffer[recv_bytes] = '\0';
    int dyn_port = atoi(buffer);
    close(sock);

    printf("[+] Puerto dinámico asignado por servidor %s: %d\n", server_ip, dyn_port);
    return dyn_port;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s <SERVER_IP> <BASE_PORT> [FILE]\n", argv[0]);
        printf("Si FILE no se pasa, se hará consulta de estado.\n");
        return 1;
    }

    const char *server_ip = argv[1];
    int base_port = atoi(argv[2]);
    const char *filename = (argc >= 4) ? argv[3] : NULL;

    int dyn_port = connect_dynamic_port(server_ip, base_port);
    if (dyn_port <= 0) return 1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(dyn_port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        logStatus(server_ip, dyn_port, "Fallo conexión al puerto dinámico", filename);
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];

    if (filename) {
        // Enviar archivo
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
        logStatus(server_ip, dyn_port, "Archivo enviado y eco recibido", filename);
    } else {
        // Consulta de estado
        int recv_bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (recv_bytes > 0) {
            buffer[recv_bytes] = '\0';
            printf("[Estado del servidor %s]: %s\n", server_ip, buffer);
            logStatus(server_ip, dyn_port, buffer, NULL);
        } else {
            perror("recv estado");
        }
    }

    close(sock);
    return 0;
}