#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

typedef long long ll;

/*
Instrucciones de compilación y uso

Compilación (macOS con clang):
    clang -Wall -Wextra -O2 P5/SUC/client.c -o client

Compilación (Linux con gcc):
    gcc -Wall -Wextra -O2 P5/SUC/client.c -o client

Ejecución básica:
    ./client 127.0.0.1 9200 0 /ruta/al/archivo.txt
    # Debe imprimirse: "Archivo enviado correctamente."

Protocolo de aplicación:
    Cabecera que envía el cliente (una línea): "INDICE FILENAME SIZE\n"
    Espera del servidor: "READY\n" para comenzar a transmitir bytes
*/

static ssize_t read_line(int fd, char *out, size_t cap) {
    // Lee del socket hasta encontrar "\n" o agotar el buffer
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) break; // EOF
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        out[used++] = c;
        if (c == '\n') break;
    }
    out[used] = '\0';
    return (ssize_t)used;
}

static int send_all(int fd, const void *buf, size_t len) {
    // Envía exactamente len bytes manejando envíos parciales
    size_t sent = 0;
    const char *p = (const char*)buf;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <host> <port> <server_index> <file_path>\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 5) { usage(argv[0]); return 1; }
    const char *host = argv[1];
    int port = atoi(argv[2]);
    int server_index = atoi(argv[3]);
    const char *file_path = argv[4];

    struct stat st;
    if (stat(file_path, &st) != 0) {
        perror("stat");
        return 1;
    }
    ll fsize = (ll)st.st_size;

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); close(fd); return 1; }

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        perror("inet_pton"); close(fd); close(s); return 1; }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); close(s); return 1; }

    // Obtener el nombre de archivo desde la ruta
    const char *slash = strrchr(file_path, '/');
    const char *fname = slash ? slash + 1 : file_path;

    // Enviar cabecera: "INDICE FILENAME SIZE\n"
    char header[1024];
    int hl = snprintf(header, sizeof(header), "%d %s %lld\n", server_index, fname, (long long)fsize);
    if (hl <= 0 || hl >= (int)sizeof(header)) {
        fprintf(stderr, "Cabecera demasiado larga\n");
        close(fd); close(s); return 1;
    }
    if (send_all(s, header, (size_t)hl) != 0) {
        perror("send header"); close(fd); close(s); return 1; }

    // Esperar READY\n del servidor (permiso para empezar a enviar bytes)
    char line[256];
    ssize_t rn = read_line(s, line, sizeof(line));
    if (rn <= 0) { fprintf(stderr, "Sin READY del servidor\n"); close(fd); close(s); return 1; }
    if (strncmp(line, "READY", 5) != 0) {
        fprintf(stderr, "Respuesta del servidor: %s", line);
        close(fd); close(s); return 1;
    }

    // Enviar el archivo por bloques
    const size_t BUF = 64 * 1024;
    char *buffer = (char*)malloc(BUF);
    if (!buffer) { close(fd); close(s); return 1; }
    ll remaining = fsize;
    while (remaining > 0) {
        size_t chunk = (remaining > (ll)BUF) ? BUF : (size_t)remaining;
        ssize_t n = read(fd, buffer, chunk);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read file"); free(buffer); close(fd); close(s); return 1;
        }
        if (n == 0) break; // EOF
        if (send_all(s, buffer, (size_t)n) != 0) {
            perror("send data"); free(buffer); close(fd); close(s); return 1;
        }
        remaining -= (ll)n;
    }
    free(buffer);
    close(fd);
    close(s);
    printf("Archivo enviado correctamente.\n");
    return 0;
}


