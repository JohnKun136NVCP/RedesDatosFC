// client.c — Práctica III
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>

#define HDR_MAX 512

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, buf + sent, len - sent, 0);
        if (r < 0) return -1;
        sent += (size_t)r;
    }
    return 0;
}

static ssize_t recv_line(int fd, char *out, size_t max) {
    size_t pos = 0;
    while (pos + 1 < max) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) return 0;  // peer cerró
        if (r < 0) return -1;
        out[pos++] = c;
        if (c == '\n') break;
    }
    out[pos] = '\0';
    return (ssize_t)pos;
}

static int connect_and_send(const char *ip, int port,
                            int req_port, int shift,
                            const char *data, size_t n) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &srv.sin_addr) != 1) {
        fprintf(stderr, "inet_pton fallo para %s\n", ip);
        close(s); return -1;
    }

    if (connect(s, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        fprintf(stderr, "[!] No se pudo conectar a %s:%d (%s)\n", ip, port, strerror(errno));
        close(s); return -1;
    }

    // Protocolo: "REQPORT <req_port> SHIFT <shift> SIZE <n>\n"
    char hdr[HDR_MAX];
    int hl = snprintf(hdr, sizeof(hdr), "REQPORT %d SHIFT %d SIZE %zu\n",
                      req_port, shift, n);
    if (hl <= 0 || hl >= (int)sizeof(hdr)) {
        fprintf(stderr, "header demasiado largo\n");
        close(s); return -1;
    }

    if (send_all(s, hdr, (size_t)hl) < 0 || send_all(s, data, n) < 0) {
        fprintf(stderr, "Fallo enviando a %s:%d\n", ip, port);
        close(s); return -1;
    }

    char resp[128];
    if (recv_line(s, resp, sizeof(resp)) <= 0) {
        fprintf(stderr, "Sin respuesta de %s:%d\n", ip, port);
        close(s); return -1;
    }

    // Imprime respuesta del servidor
    // Se espera "ACCEPTED" o "REJECTED"
    // (El servidor termina con '\n'.)
    printf("[+] SERVER RESPONSE %d: %s", port, resp);

    close(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <SERVIDOR_IP> <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <archivo.txt>\n", argv[0]);
        fprintf(stderr, "Ej:  %s 192.168.0.197 49202 30 acontecimiento.txt\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int req_port   = atoi(argv[2]);   // puerto que debe procesar
    int shift      = atoi(argv[3]);
    const char *path = argv[4];

    // Leer archivo completo
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0) {
        fprintf(stderr, "Archivo invalido: %s\n", path);
        return 1;
    }
    size_t n = (size_t)st.st_size;
    char *data = (char*)malloc(n);
    if (!data) { fprintf(stderr, "memoria insuficiente\n"); return 1; }

    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); free(data); return 1; }
    if (fread(data, 1, n, f) != n) { fprintf(stderr, "Error leyendo archivo\n"); fclose(f); free(data); return 1; }
    fclose(f);

    // Lista de puertos del servidor para iterar
    int ports[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; ++i) {
        int port = ports[i];
        printf("[*] Connecting to %s:%d ...\n", ip, port);
        connect_and_send(ip, port, req_port, shift, data, n);
    }

    free(data);
    printf("[*] Done.\n");
    return 0;
}
