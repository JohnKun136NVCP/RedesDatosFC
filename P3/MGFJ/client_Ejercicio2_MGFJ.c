#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Cambio para MULTI: agregamos timeouts por operación
#include <sys/time.h>

#define SEND_BUFSZ 8192
#define RECV_BUFSZ 8192

static ssize_t recv_line(int fd, char *line, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        char c; ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) { if (i == 0) return 0; break; }
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        line[i++] = c;
        if (c == '\n') break;
    }
    line[i] = '\0';
    return (ssize_t)i;
}

// Conexión al servidor TCP
static int connect_to(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) {
        perror("inet_pton"); close(s); return -1;
    }
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect"); close(s); return -1;
    }

    // Cambio para MULTI: configuramos timeouts de RCV/SND
    struct timeval tv; tv.tv_sec = 30; tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    return s;
}

static int send_file_request(int sock, int target_port, int shift, const char *filepath) {
    const char *base = filepath;
    const char *slash = strrchr(filepath, '/');
    if (slash && *(slash + 1) != '\0') base = slash + 1;

    dprintf(sock, "TARGET %d\n", target_port);
    dprintf(sock, "SHIFT %d\n", shift);
    dprintf(sock, "NAME %s\n", base);
    dprintf(sock, "\n");

    FILE *fp = fopen(filepath, "rb");
    if (!fp) { perror("fopen"); return -1; }

    char buf[SEND_BUFSZ];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        size_t sent = 0;
        while (sent < n) {
            ssize_t w = send(sock, buf + sent, n - sent, 0);
            if (w < 0) { if (errno == EINTR) continue; perror("send"); fclose(fp); return -1; }
            sent += (size_t)w;
        }
    }
    fclose(fp);

    shutdown(sock, SHUT_WR);
    return 0;
}

// Cambio para MULTI: devolvemos también el NAME al main
// 0 = OK guardado; 1 = REJECT; -1 = error
static int recv_response_and_save(int sock, int port, char *outname_buf, size_t outname_sz) {
    char line[4096];

    ssize_t r = recv_line(sock, line, sizeof(line));
    if (r <= 0) { fprintf(stderr, "[-] SERVER RESPONSE %d: no status from server\n", port); return -1; }

    int srv_port = -1; bool ok = false;
    if (sscanf(line, "OK %d", &srv_port) == 1) {
        ok = true;
    } else if (sscanf(line, "REJECT %d", &srv_port) == 1) {
        return 1;
    } else {
        fprintf(stderr, "[-] SERVER RESPONSE %d: bad status line: %s", port, line);
        return -1;
    }

    if (!ok) return 1;

    r = recv_line(sock, line, sizeof(line));
    if (r <= 0) { fprintf(stderr, "[-] SERVER RESPONSE %d: NAME missing\n", port); return -1; }

    char outname[600] = {0};
    if (sscanf(line, "NAME %599[^\n]", outname) != 1) {
        fprintf(stderr, "[-] SERVER RESPONSE %d: bad NAME line: %s", port, line);
        return -1;
    }

    // Cambio para MULTI: copiamos el nombre para imprimirlo en el mensaje final
    if (outname_buf && outname_sz) {
        strncpy(outname_buf, outname, outname_sz - 1);
        outname_buf[outname_sz - 1] = '\0';
    }

    r = recv_line(sock, line, sizeof(line));
    if (r < 0) { fprintf(stderr, "[-] SERVER RESPONSE %d: blank line missing\n", port); return -1; }

    FILE *fo = fopen(outname, "wb");
    if (!fo) { perror("[-] SERVER RESPONSE output fopen"); return -1; }

    char buf[RECV_BUFSZ];
    for (;;) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; perror("[-] SERVER RESPONSE recv"); fclose(fo); return -1; }
        fwrite(buf, 1, (size_t)n, fo);
    }
    fclose(fo);
    return 0;
}

// Cambio para MULTI: main acepta múltiples PUERTOS y ARCHIVOS
int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage:\n  %s <SERVER_IP> <PORT1> ... <PORTk> <FILE1> ... <FILEm> <SHIFT>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int last = argc - 1;  // SHIFT

    char *endp = NULL;
    (void)strtol(argv[last], &endp, 10);
    if (!endp || *endp != '\0') {
        fprintf(stderr, "[-] Invalid SHIFT: %s\n", argv[last]);
        return 1;
    }
    int shift = atoi(argv[last]);

    // Cambio para MULTI: detectar frontera entre puertos y archivos
    int first_data = 2;
    int first_file = first_data;
    for (; first_file < last; ++first_file) {
        char *e = NULL; long v = strtol(argv[first_file], &e, 10);
        if (!e || *e != '\0' || v <= 0 || v > 65535) break; // aquí empiezan los archivos
    }

    int k = first_file - first_data; // puertos
    int m = last - first_file;       // archivos
    if (k <= 0 || m <= 0 || k > 3 || m > 3) {
        fprintf(stderr, "Usage (1..3):\n  %s <IP> <P1..Pk> <F1..Fm> <S>\n", argv[0]);
        return 1;
    }

    char **ports_argv = &argv[first_data];
    char **files_argv = &argv[first_file];

    // Cambio para MULTI: bucle para enviar a cada puerto
    for (int i = 0; i < k; ++i) {
        int port = atoi(ports_argv[i]);
        const char *file = files_argv[i % m];   // reutiliza archivo si faltan

        int sock = connect_to(server_ip, port);
        if (sock < 0) {
            fprintf(stderr, "[-] SERVER RESPONSE %d: could not connect\n", port);
            continue;
        }

        if (send_file_request(sock, port, shift, file) < 0) {
            fprintf(stderr, "[-] SERVER RESPONSE %d: send error (file %s)\n", port, file);
            close(sock);
            continue;
        }

        // Cambio para MULTI: mostrar el nombre final del archivo
        char outname[600] = {0};
        int rc = recv_response_and_save(sock, port, outname, sizeof(outname));
        if (rc == 0) {
            printf("[+] SERVER RESPONSE %d: received and encrypted as %s\n", port, outname);
        } else if (rc == 1) {
            printf("[-] SERVER RESPONSE %d: REJECTED\n", port);
        } else {
            printf("[-] SERVER RESPONSE %d: ERROR\n", port);
        }

        close(sock);
    }

    return 0;
}
