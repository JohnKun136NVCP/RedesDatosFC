
// Cliente multi: acepta varios puertos y varios archivos en una sola corrida.
// Uso esperado (shift al final):
// Leemos puertos mientras sean tokens numericos, el ultimo token es numérico (shift).
// Protocolo:
//   envio: "PORT <p> SHIFT <s> NAME <fname> SIZE <n>\n" + n bytes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define MAX_FILE (8*1024*1024)

// Filtramos (limpiamos)
static int is_number(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static int send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t w = send(fd, (const char*)buf + sent, len - sent, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)w;
    }
    return 1;
}

static ssize_t recv_line(int fd, char *out, size_t cap) {
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        out[used++] = c;
        if (c == '\n') break;
    }
    out[used] = '\0';
    return (ssize_t)used;
}

static int read_file(const char *path, char **buf, size_t *len) {
    struct stat st;
    if (stat(path, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        fprintf(stderr, "Archivo inválido: %s\n", path);
        return 0;
    }
    if (st.st_size > MAX_FILE) {
        fprintf(stderr, "Archivo muy grande para la práctica: %s\n", path);
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 0; }
    size_t n = (size_t)st.st_size;
    char *tmp = (char*)malloc(n ? n : 1);
    if (!tmp) { fclose(f); return 0; }
    if (n && fread(tmp, 1, n, f) != n) {
        fprintf(stderr, "Error leyendo archivo: %s\n", path);
        free(tmp); fclose(f); return 0;
    }
    fclose(f);
    *buf = tmp; *len = n;
    return 1;
}

static int connect_and_send(const char *ip, int port, const char *file, int shift) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 0; }

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        fprintf(stderr, "IP inválida: %s\n", ip);
        close(fd); return 0;
    }

    if (connect(fd, (struct sockaddr*)&a, sizeof(a)) < 0) {
        printf("[x] No se pudo conectar a %s:%d ...\n", ip, port);
        close(fd); return 0;
    }

    char *data = NULL; size_t n = 0;
    if (!read_file(file, &data, &n)) {
        close(fd); return 0;
    }

    // Cabecera
    char hdr[MAX_LINE];
    snprintf(hdr, sizeof(hdr), "PORT %d SHIFT %d NAME %s SIZE %zu\n", port, shift, file, n);
    if (send_all(fd, hdr, strlen(hdr)) < 0) { close(fd); free(data); return 0; }
    if (n && send_all(fd, data, n) < 0)      { close(fd); free(data); return 0; }

    char resp[128];
    if (recv_line(fd, resp, sizeof(resp)) <= 0) {
        printf("[!] Cliente %d: sin respuesta\n", port);
        close(fd); free(data); return 0;
    }

    if (strncmp(resp, "OK", 2) == 0) {
        printf("[Cliente] Puerto %d: ARCHIVO CIFRADO RECIBIDO\n", port);
    } else {
        printf("[Cliente] Puerto %d: RECHAZADO\n", port);
    }

    free(data);
    close(fd);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Uso:\n  %s <IP> <puerto1> [puerto2 ...] <file1> [file2 ...] <shift>\n"
            "Ejemplo:\n  %s 192.168.0.197 49200 49201 49202 file1.txt file2.txt 30\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *ip = argv[1];

    // El ultimo argumento es el shift
    if (!is_number(argv[argc-1])) {
        fprintf(stderr, "El último argumento debe ser SHIFT numérico.\n");
        return 1;
    }
    int shift = atoi(argv[argc-1]);

    // A partir de argv[2] hay una lista de puertos (tokens numericos) seguida de archivos (no numericos)
    int first_port_idx = 2;
    int first_file_idx = -1;

    for (int i = first_port_idx; i < argc-1; ++i) {           // excluye el shift final
        if (!is_number(argv[i])) { first_file_idx = i; break; }
    }
    if (first_file_idx < 0) {
        fprintf(stderr, "Debes especificar al menos un archivo antes del shift.\n");
        return 1;
    }

    // Rangos:
    //  puertos: [first_port_idx, first_file_idx)
    //  archivos: [first_file_idx, argc-1)  
    int n_ports = first_file_idx - first_port_idx;
    int n_files = (argc-1) - first_file_idx;

    if (n_ports <= 0 || n_files <= 0) {
        fprintf(stderr, "Faltan puertos o archivos.\n");
        return 1;
    }

    // Imprime lo que voy a hacer 
    printf("[*] Conectando a %s con shift=%d\n", ip, shift);
    printf("[*] Puertos: ");
    for (int i = 0; i < n_ports; ++i) printf("%s ", argv[first_port_idx + i]);
    printf("\n[*] Archivos: ");
    for (int j = 0; j < n_files; ++j) printf("%s ", argv[first_file_idx + j]);
    printf("\n");

    // Dispara combinaciones puerto por archivo
    for (int i = 0; i < n_ports; ++i) {
        int port = atoi(argv[first_port_idx + i]);
        for (int j = 0; j < n_files; ++j) {
            const char *file = argv[first_file_idx + j];
            (void)connect_and_send(ip, port, file, shift);
        }
    }

    printf("[✓] Done.\n");
    return 0;
}
