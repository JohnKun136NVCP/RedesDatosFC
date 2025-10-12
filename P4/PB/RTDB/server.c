#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

void logStatus(const char *alias, const char *client_ip, int client_port, const char *status) {
    char logFile[128];
    snprintf(logFile, sizeof(logFile), "%s/server_log.txt", alias);
    FILE *log = fopen(logFile, "a");
    if (log) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strcspn(ts, "\n")] = 0;
        fprintf(log, "[%s] Cliente %s:%d Estado: %s\n", ts, client_ip, client_port, status);
        fclose(log);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <alias>\nEjemplo: %s s01\n", argv[0], argv[0]);
        return 1;
    }

    const char *alias = argv[1];
    int base_port = 49200;
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];

    mkdir(alias, 0777);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); exit(1); }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(base_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen"); exit(1);
    }

    printf("[+] Servidor %s escuchando en puerto base %d...\n", alias, base_port);

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) { perror("accept"); continue; }

        printf("[+] Conexión inicial de %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Revisar si el cliente solo quiere status
        int peek_bytes = recv(client_sock, buffer, sizeof(buffer)-1, MSG_PEEK | MSG_DONTWAIT);
        if (peek_bytes == 0) {
            char *status_msg = "ESPERA";
            send(client_sock, status_msg, strlen(status_msg), 0);
            logStatus(alias, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), "Cliente de status atendido");
            close(client_sock);
            continue;
        }

        // Asignar puerto dinámico > base_port
        int dyn_port_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (dyn_port_sock < 0) { perror("socket"); close(client_sock); continue; }

        struct sockaddr_in dyn_addr;
        dyn_addr.sin_family = AF_INET;
        dyn_addr.sin_port = 0; // dejar que el SO elija un puerto
        dyn_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(dyn_port_sock, (struct sockaddr*)&dyn_addr, sizeof(dyn_addr)) < 0) {
            perror("bind"); close(dyn_port_sock); close(client_sock); continue;
        }

        socklen_t len = sizeof(dyn_addr);
        if (getsockname(dyn_port_sock, (struct sockaddr*)&dyn_addr, &len) < 0) {
            perror("getsockname"); close(dyn_port_sock); close(client_sock); continue;
        }
        int assigned_port = ntohs(dyn_addr.sin_port);

        if (listen(dyn_port_sock, 1) < 0) { perror("listen"); close(dyn_port_sock); close(client_sock); continue; }

        // Enviar puerto dinámico al cliente
        char port_msg[16];
        snprintf(port_msg, sizeof(port_msg), "%d", assigned_port);
        send(client_sock, port_msg, strlen(port_msg), 0);
        close(client_sock); // Cerrar puerto base

        printf("[+] Puerto dinámico asignado: %d\n", assigned_port);

        // Esperar conexión en puerto dinámico
        int file_sock = accept(dyn_port_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (file_sock < 0) { perror("accept"); close(dyn_port_sock); continue; }

        char outFile[128];
        snprintf(outFile, sizeof(outFile), "%s/received_file.txt", alias);
        FILE *fp = fopen(outFile, "wb");
        if (!fp) { perror("fopen"); close(file_sock); close(dyn_port_sock); continue; }

        int bytes;
        while ((bytes = recv(file_sock, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes, fp);
            send(file_sock, buffer, bytes, 0);
        }
        fclose(fp);
        logStatus(alias, inet_ntoa(client_addr.sin_addr), assigned_port, "Archivo recibido y reenviado");

        printf("[+] Archivo guardado en %s\n", outFile);
        close(file_sock);
        close(dyn_port_sock);
    }

    close(server_sock);
    return 0;
}
