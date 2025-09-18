#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF 1024

static int read_line(int fd, char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return i;
}
static int read_n(int fd, char *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = recv(fd, buf + got, n - got, 0);
        if (r <= 0) return r;
        got += r;
    }
    return got;
}
static void log_status(const char *state) {
    FILE *fp = fopen("status.log", "a");
    if (!fp) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[64];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", tm);
    fprintf(fp, "%s | %s\n", ts, state);
    fclose(fp);
    printf("%s | %s\n", ts, state);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <IP_SERVIDOR|alias> <PUERTO_BASE> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int target = atoi(argv[2]);
    const char *path = argv[3];

    // leer archivo
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fprintf(stderr, "Archivo vacío\n"); fclose(f); return 1; }
    char *body = (char*)malloc(len);
    if (!body) { fprintf(stderr, "sin memoria\n"); fclose(f); return 1; }
    if (fread(body, 1, len, f) != (size_t)len) { perror("fread"); free(body); fclose(f); return 1; }
    fclose(f);

    // 1) conectar a puerto base
    int s0 = socket(AF_INET, SOCK_STREAM, 0);
    if (s0 < 0) { perror("socket"); free(body); return 1; }

    struct sockaddr_in a0;
    memset(&a0, 0, sizeof(a0));
    a0.sin_family = AF_INET;
    a0.sin_port = htons(target);

    // resolver alias/IP 
    struct hostent *he = gethostbyname(server_ip);
    if (he && he->h_addr_list && he->h_addr_list[0]) {
        memcpy(&a0.sin_addr, he->h_addr_list[0], he->h_length);
    } else if (inet_pton(AF_INET, server_ip, &a0.sin_addr) != 1) {
        perror("resolver");
        close(s0); free(body); return 1;
    }

    if (connect(s0, (struct sockaddr*)&a0, sizeof(a0)) < 0) {
        perror("connect base");
        close(s0); free(body); return 1;
    }
    log_status("conectado a PUERTO_BASE; solicitando puerto asignado");

    // leer "PORT:<n>\n\n"
    char line[BUF];
    if (read_line(s0, line, sizeof line) <= 0) { fprintf(stderr, "sin respuesta\n"); close(s0); free(body); return 1; }
    int newport = 0; sscanf(line, "PORT:%d", &newport);
    // línea en blanco tolerante
    read_line(s0, line, sizeof line);
    close(s0);

    if (newport <= 0) { fprintf(stderr, "puerto asignado inválido\n"); free(body); return 1; }
    char msg[128]; snprintf(msg, sizeof msg, "puerto asignado = %d", newport);
    log_status(msg);

    // 2) reconectar a puerto asignado
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); free(body); return 1; }
    a0.sin_port = htons(newport);
    if (connect(s, (struct sockaddr*)&a0, sizeof(a0)) < 0) {
        perror("connect asignado");
        close(s); free(body); return 1;
    }
    log_status("conectado a puerto asignado; enviando archivo");

    // enviar encabezado + cuerpo
    char hdr[256];
    const char *fname = strrchr(path, '/'); fname = fname ? fname + 1 : path;
    int n = snprintf(hdr, sizeof hdr, "LEN:%ld\nNAME:%s\n\n", len, fname);
    if (send(s, hdr, n, 0) != n) { perror("send hdr"); close(s); free(body); return 1; }

    long sent = 0;
    while (sent < len) {
        ssize_t w = send(s, body + sent, len - sent, 0);
        if (w <= 0) { perror("send body"); close(s); free(body); return 1; }
        sent += w;
    }

    // respuesta
    if (read_line(s, line, sizeof line) > 0) {
        if (!strncmp(line, "OK", 2)) log_status("transmitido OK");
        else log_status("error en servidor");
    } else {
        log_status("sin respuesta del servidor");
    }

    close(s);
    free(body);
    return 0;
}