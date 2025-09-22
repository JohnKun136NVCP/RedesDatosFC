#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
static const int PORTS[3] = {49200, 49201, 49202};

static ssize_t send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

static char* read_and_sanitize(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    struct stat st;
    if (stat(path, &st) != 0) { perror("stat"); fclose(f); return NULL; }
    size_t cap = (size_t)st.st_size + 1;
    char *raw = (char*)malloc(cap);
    if (!raw) { fclose(f); return NULL; }
    size_t n = fread(raw, 1, cap - 1, f);
    raw[n] = '\0';
    fclose(f);

    char *clean = (char*)malloc(n + 1);
    if (!clean) { free(raw); return NULL; }
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)raw[i];
        if (isalpha(c)) clean[j++] = (char)tolower(c);
    }
    clean[j] = '\0';
    free(raw);
    return clean;
}

static int connect_to(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton: IP inválida: %s\n", ip);
        close(sock); return -1;
    }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(sock); return -1;
    }
    return sock;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO_DESTINO> <DESPLAZAMIENTO> <RUTA_ARCHIVO>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port_dst = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *path = argv[4];

    char *content = read_and_sanitize(path);
    if (!content) { fprintf(stderr, "No se pudo leer/sanitizar %s\n", path); return 1; }

    size_t msg_len = strlen(content) + 64;
    char *message = (char*)malloc(msg_len);
    if (!message) { free(content); return 1; }
    snprintf(message, msg_len, "%d %d %s", port_dst, shift, content);

    char recvbuf[BUFFER_SIZE];

    for (size_t i = 0; i < 3; ++i) {
        int srv_port = PORTS[i];
        int sock = connect_to(ip, srv_port);
        if (sock < 0) { fprintf(stderr, "Conexión fallida al puerto %d\n", srv_port); continue; }
        if (send_all(sock, message, strlen(message)) < 0) { perror("send_all"); close(sock); continue; }
        ssize_t n = recv(sock, recvbuf, sizeof(recvbuf)-1, 0);
        if (n > 0) { recvbuf[n] = '\0'; printf("Servidor %d → %s\n", srv_port, recvbuf); }
        else       { printf("Servidor %d → sin respuesta\n", srv_port); }
        close(sock);
    }

    free(message);
    free(content);
    return 0;
}
