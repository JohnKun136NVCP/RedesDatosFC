// Cliente TCP multi-destino.
// Uso:
//   clientMulti <SERVIDOR_IP> <SHIFT> <puerto1>:<archivo1> [<puerto2>:<archivo2> ...]
// Ejemplo:
//   clientMulti 127.0.0.1 10 49200:mensaje.txt 49201:otro.txt 49202:tercero.txt
// Envia para cada par puerto:archivo el contenido con el mismo SHIFT.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECV_BUF 1024

static int connect_tcp(const char *server_ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return -1; }
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Dirección IP inválida: %s\n", server_ip); close(sockfd); return -1; }
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(sockfd); return -1; }
    return sockfd;
}

static int send_all(int sockfd, const void *data, size_t len) {
    const char *p = (const char *)data; size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sockfd, p + total, len - total, 0);
        if (sent < 0) { if (errno == EINTR) continue; perror("send"); return -1; }
        if (sent == 0) { fprintf(stderr, "Conexión cerrada al enviar.\n"); return -1; }
        total += (size_t)sent;
    }
    return 0;
}

static char *read_file_into_buffer(const char *path, size_t *out_size) {
    FILE *fp = fopen(path, "rb"); if (!fp) { perror("fopen"); return NULL; }
    if (fseek(fp, 0, SEEK_END) != 0) { perror("fseek"); fclose(fp); return NULL; }
    long size = ftell(fp); if (size < 0) { perror("ftell"); fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { perror("fseek"); fclose(fp); return NULL; }
    char *buf = (char *)malloc((size_t)size); if (!buf) { fprintf(stderr, "memoria insuficiente\n"); fclose(fp); return NULL; }
    size_t nread = fread(buf, 1, (size_t)size, fp);
    if (nread != (size_t)size) { fprintf(stderr, "Error al leer archivo (leídos %zu de %ld)\n", nread, size); free(buf); fclose(fp); return NULL; }
    fclose(fp); if (out_size) *out_size = (size_t)size; return buf;
}

static int sum_digits(const char *s) {
    int sum = 0; for (const char *p = s; *p; ++p) if (isdigit((unsigned char)*p)) sum += (*p - '0');
    return sum;
}

static int parse_shift_arg(const char *arg) {
    if (strncmp(arg, "auto", 4) == 0) {
        const char *colon = strchr(arg, ':');
        if (colon && colon[1] != '\0') return sum_digits(colon + 1);
        return 0;
    }
    return atoi(arg);
}

static char *filter_alpha_only(const char *in, size_t in_size, size_t *out_size) {
    if (!in) return NULL; char *out = (char *)malloc(in_size); if (!out) return NULL; size_t w = 0;
    for (size_t i = 0; i < in_size; i++) { unsigned char c = (unsigned char)in[i]; if (isalpha(c)) out[w++] = (char)c; }
    if (out_size) *out_size = w; return out;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <SERVIDOR_IP> <SHIFT> <puerto:archivo> [puerto:archivo ...]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 10 49200:mensaje.txt 49201:otro.txt 49202:tercero.txt\n", argv[0]);
        return 1;
    }
    const char *server_ip = argv[1];
    int shift = parse_shift_arg(argv[2]);

    for (int i = 3; i < argc; i++) {
        char *spec = argv[i];
        char *colon = strchr(spec, ':');
        if (!colon) {
            fprintf(stderr, "Arg inválido (esperado puerto:archivo): %s\n", spec);
            continue;
        }
        *colon = '\0';
        int port = atoi(spec);
        const char *file_path = colon + 1;

        size_t raw_size = 0; char *raw_data = read_file_into_buffer(file_path, &raw_size);
        if (!raw_data) { *colon = ':'; continue; }
        size_t clean_size = 0; char *file_data = filter_alpha_only(raw_data, raw_size, &clean_size);
        free(raw_data);
        if (!file_data) { fprintf(stderr, "Error al limpiar %s\n", file_path); *colon = ':'; continue; }

        int sockfd = connect_tcp(server_ip, port);
        if (sockfd < 0) { free(file_data); *colon = ':'; continue; }

        char header[256];
        int header_len = snprintf(header, sizeof(header),
                                  "TARGET_PORT %d\nSHIFT %d\nCONTENT_LENGTH %zu\n\n",
                                  port, shift, clean_size);
        if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
            fprintf(stderr, "Error al construir encabezado para %s:%d\n", file_path, port);
            close(sockfd); free(file_data); *colon = ':'; continue;
        }

        if (send_all(sockfd, header, (size_t)header_len) < 0 ||
            send_all(sockfd, file_data, clean_size) < 0) {
            close(sockfd); free(file_data); *colon = ':'; continue;
        }
        free(file_data);

        char resp[RECV_BUF]; ssize_t r = recv(sockfd, resp, sizeof(resp) - 1, 0);
        if (r < 0) { perror("recv"); close(sockfd); *colon = ':'; continue; }
        resp[r > 0 ? (size_t)r : 0] = '\0';

        if (strstr(resp, "PROCESSED") != NULL) {
            printf("[CLIENT-M] %d:%s -> PROCESSED\n", port, file_path);
        } else if (strstr(resp, "REJECTED") != NULL) {
            printf("[CLIENT-M] %d:%s -> REJECTED\n", port, file_path);
        } else {
            printf("[CLIENT-M] %d:%s -> Respuesta desconocida: %s\n", port, file_path, resp);
        }
        close(sockfd);
        *colon = ':';
    }
    return 0;
}


