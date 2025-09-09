// clientMulti.c – múltiples puertos y archivos
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct _attribute_((packed)) Header {
    uint32_t target_port_be;
    int32_t  shift_be;
    uint32_t data_len_be;
};

static int is_number(const char *s) {
    if (*s=='\0') return 0;
    for (const char *p=s; *p; ++p) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static ssize_t sendall(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, (const char*)buf + sent, len - sent, 0);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; return -1; }
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}
static ssize_t recvall(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, (char*)buf + got, len - got, 0);
        if (r == 0) return (ssize_t)got;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return (ssize_t)got;
}
static int connect_to(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { close(s); return -1; }
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) { close(s); return -1; }
    return s;
}

static int send_file_to_port(const char *ip, int port, int shift, const char *filepath) {
    // lee archivo completo
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long L = ftell(fp);
    if (L < 0) { fclose(fp); return 0; }
    fseek(fp, 0, SEEK_SET);
    char buf = (char)malloc((size_t)L + 1);
    if (!buf) { fclose(fp); return 0; }
    size_t rd = fread(buf, 1, (size_t)L, fp);
    fclose(fp);
    if (rd != (size_t)L) { free(buf); return 0; }
    buf[L] = '\0';

    int s = connect_to(ip, port);
    if (s < 0) { free(buf); return 0; }

    struct Header h;
    h.target_port_be = htonl((uint32_t)port);  // el objetivo es ese puerto
    h.shift_be       = htonl(shift);
    h.data_len_be    = htonl((uint32_t)L);

    int ok = 0;
    if (sendall(s, &h, sizeof(h)) >= 0 && sendall(s, buf, (size_t)L) >= 0) {
        uint32_t resp_be = 0, resp = 0;
        if (recvall(s, &resp_be, sizeof(resp_be)) == (ssize_t)sizeof(resp_be)) {
            resp = ntohl(resp_be);
        }
        if (resp == 1) ok = 1;
    }
    close(s);
    free(buf);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
          "Uso:\n  %s <IP> <P1> ... <Pk> <F1> ... <Fk> <SHIFT>\n"
          "Ejemplo:\n  %s 192.168.0.193 49200 49201 49202 file1.txt file2.txt file3.txt 49\n"
          "Multiples archivos por puerto: usa comas a.txt,b.txt\n",
          argv[0], argv[0]);
        return 1;
    }

    const char *ip = argv[1];

    // Encuentra el límite entre puertos y archivos (primer token no numérico)
    int first_file_idx = -1;
    for (int i = 2; i < argc - 1; ++i) { // -1: reservamos último para shift
        if (!is_number(argv[i])) { first_file_idx = i; break; }
    }
    if (first_file_idx == -1) {
        fprintf(stderr, "Error: no se detectaron archivos.\n");
        return 1;
    }

    int port_count = first_file_idx - 2;
    int file_count = (argc - 1) - first_file_idx; // último es shift
    if (port_count <= 0 || file_count <= 0 || port_count != file_count) {
        fprintf(stderr, "Error: cantidad de puertos (%d) y archivos (%d) debe coincidir.\n",
                port_count, file_count);
        return 1;
    }

    int shift = atoi(argv[argc - 1]);

    // Procesa cada (PORTi, FILEi). Si FILEi lleva comas, envía cada archivo.
    for (int i = 0; i < port_count; ++i) {
        int port = atoi(argv[2 + i]);
        char *filespec = argv[first_file_idx + i];

        // duplicamos para poder hacer strtok sin modificar argv
        char *tmp = strdup(filespec);
        if (!tmp) { printf("[Cliente] Puerto %d: RECHAZADO\n", port); continue; }

        int any_ok = 0;
        for (char *tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
            int ok = send_file_to_port(ip, port, shift, tok);
            if (ok) any_ok = 1;
        }
        free(tmp);

        if (any_ok) {
            printf("[Cliente] Puerto %d: ARCHIVO CIFRADO RECIBIDO\n", port);
        } else {
            printf("[Cliente] Puerto %d: RECHAZADO\n", port);
        }
    }

    return 0;
}
