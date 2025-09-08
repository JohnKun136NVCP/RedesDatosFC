// Cliente TCP para enviar un archivo de texto a un servidor
// Protocolo simple:
//   Header (líneas terminadas en '\n'):
//     TARGET_PORT <puerto_objetivo>\n
//     SHIFT <desplazamiento>\n
//     CONTENT_LENGTH <n>\n
//   Línea en blanco: "\n"
//   Cuerpo: n bytes del contenido del archivo
// El servidor responde con una sola línea: "PROCESSED" o "REJECTED" (y opcionalmente más datos en versiones futuras).

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
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Dirección IP inválida: %s\n", server_ip);
        close(sockfd);
        return -1;
    }
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

static int send_all(int sockfd, const void *data, size_t len) {
    const char *p = (const char *)data;
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sockfd, p + total, len - total, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }
        if (sent == 0) {
            fprintf(stderr, "Conexión cerrada al enviar.\n");
            return -1;
        }
        total += (size_t)sent;
    }
    return 0;
}

static char *read_file_into_buffer(const char *path, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        perror("ftell");
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)size);
    if (!buf) {
        fprintf(stderr, "memoria insuficiente\n");
        fclose(fp);
        return NULL;
    }
    size_t nread = fread(buf, 1, (size_t)size, fp);
    if (nread != (size_t)size) {
        fprintf(stderr, "Error al leer archivo (leídos %zu de %ld)\n", nread, size);
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if (out_size) *out_size = (size_t)size;
    return buf;
}

// Suma los dígitos de una cadena (por ejemplo, número de cuenta)
static int sum_digits(const char *s) {
    int sum = 0;
    for (const char *p = s; *p; ++p) {
        if (isdigit((unsigned char)*p)) sum += (*p - '0');
    }
    return sum;
}

// Parsea el argumento de shift: acepta entero o "auto:<digits>"
static int parse_shift_arg(const char *arg) {
    if (strncmp(arg, "auto", 4) == 0) {
        const char *colon = strchr(arg, ':');
        if (colon && colon[1] != '\0') {
            return sum_digits(colon + 1);
        }
        // Si no hay dígitos provistos, deja 0 por defecto
        return 0;
    }
    return atoi(arg);
}

// Filtra el buffer manteniendo solo letras A-Z/a-z. Devuelve nuevo buffer y tamaño por out_size
static char *filter_alpha_only(const char *in, size_t in_size, size_t *out_size) {
    if (!in) return NULL;
    char *out = (char *)malloc(in_size);
    if (!out) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < in_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalpha(c)) {
            out[w++] = (char)c;
        }
    }
    if (out_size) *out_size = w;
    return out;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Archivo>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 49200 10 mensaje.txt\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = parse_shift_arg(argv[3]);
    const char *file_path = argv[4];

    size_t raw_size = 0;
    char *raw_data = read_file_into_buffer(file_path, &raw_size);
    if (!raw_data) {
        return 1;
    }
    size_t clean_size = 0;
    char *clean_data = filter_alpha_only(raw_data, raw_size, &clean_size);
    free(raw_data);
    if (!clean_data) {
        fprintf(stderr, "Error al limpiar el contenido.\n");
        return 1;
    }

    int sockfd = connect_tcp(server_ip, port);
    if (sockfd < 0) {
        free(clean_data);
        return 1;
    }

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "TARGET_PORT %d\nSHIFT %d\nCONTENT_LENGTH %zu\n\n",
                              port, shift, clean_size);
    if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
        fprintf(stderr, "Error al construir encabezado.\n");
        close(sockfd);
        free(clean_data);
        return 1;
    }

    if (send_all(sockfd, header, (size_t)header_len) < 0) {
        close(sockfd);
        free(clean_data);
        return 1;
    }
    if (send_all(sockfd, clean_data, clean_size) < 0) {
        close(sockfd);
        free(clean_data);
        return 1;
    }

    free(clean_data);

    char resp[RECV_BUF];
    ssize_t r = recv(sockfd, resp, sizeof(resp) - 1, 0);
    if (r < 0) {
        perror("recv");
        close(sockfd);
        return 1;
    }
    resp[r > 0 ? (size_t)r : 0] = '\0';

    if (strstr(resp, "PROCESSED") != NULL) {
        printf("[CLIENT] Servidor respondió: PROCESSED\n");
    } else if (strstr(resp, "REJECTED") != NULL) {
        printf("[CLIENT] Servidor respondió: REJECTED\n");
    } else {
        printf("[CLIENT] Respuesta desconocida: %s\n", resp);
    }

    close(sockfd);
    return 0;
}


