
// Servidor unico que escucha 3 puertos (49200, 49201, 49202) al mismo tiempo.
// Nota: Usamos select() para no complicarme con hilos.
// Protocolo simple (una sola peticion por conexion):
//   Cliente envia:  "PORT <p> SHIFT <s> NAME <fname> SIZE <n>\n"  seguido por n bytes (contenido)
//   El servidor acepta solo si <p> == puerto local. Si no responde "REJECTED\n".
//   Si acepta, cifra con Cesar (shift s) y guarda como "<base>_<puerto>_cesar.txt". Responde "OK\n".

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_LINE   1024
#define MAX_FILE   (8*1024*1024)   // 8MB por si acaso
#define NPORTS     3

static const int PORTS[NPORTS] = {49200, 49201, 49202};


// Filtramos lo basico para no romper el script.

static void cesar(char *buf, size_t len, int shift) {
    shift %= 26; if (shift < 0) shift += 26;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c >= 'a' && c <= 'z') {
            buf[i] = (char)('a' + (c - 'a' + shift) % 26);
        } else if (c >= 'A' && c <= 'Z') {
            buf[i] = (char)('A' + (c - 'A' + shift) % 26);
        }
        // demas caracteres se quedan como estan
    }
}

static ssize_t recv_line(int fd, char *out, size_t cap) {
    // lee hasta '\n' (incluido) o hasta cap-1
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;         // peer cerro
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

static int recv_all(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, (char*)buf + got, len - got, 0);
        if (r == 0) return 0;            // peer cerro antes
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)r;
    }
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

static void base_name(const char *path, char *out, size_t cap) {
    // saca solo el nombre base sin carpetas
    const char *p = strrchr(path, '/');
    #ifdef _WIN32
    const char *q = strrchr(path, '\\');
    if (!p || (q && q > p)) p = q;
    #endif
    p = p ? p + 1 : path;
    snprintf(out, cap, "%s", p);
}

static void handle_client(int cfd, int local_port) {
    char line[MAX_LINE];
    if (recv_line(cfd, line, sizeof(line)) <= 0) {
        close(cfd);
        return;
    }

    // Esperamos: PORT <p> SHIFT <s> NAME <fname> SIZE <n>\n
    int port = 0, shift = 0;
    char fname[256] = {0};
    size_t size = 0;

    // Nota: fname no debe llevar espacios.
    int matched = sscanf(line, "PORT %d SHIFT %d NAME %255s SIZE %zu", &port, &shift, fname, &size);

    if (matched != 4 || size > MAX_FILE) {
        const char *msg = "REJECTED\n";
        send_all(cfd, msg, strlen(msg));
        close(cfd);
        return;
    }

    if (port != local_port) {
        const char *msg = "REJECTED\n";
        send_all(cfd, msg, strlen(msg));
        close(cfd);
        return;
    }

    char *buf = (char*)malloc(size ? size : 1);
    if (!buf) {
        const char *msg = "REJECTED\n";
        send_all(cfd, msg, strlen(msg));
        close(cfd);
        return;
    }

    if (size > 0) {
        if (recv_all(cfd, buf, size) <= 0) {
            free(buf);
            close(cfd);
            return;
        }
    }

    cesar(buf, size, shift);

    // nombre de salida: "<base>_<puerto>_cesar.txt"
    char base[256]; base_name(fname, base, sizeof(base));
    char outname[512];
    snprintf(outname, sizeof(outname), "%s_%d_cesar.txt", base, local_port);

    FILE *fo = fopen(outname, "wb");
    if (fo) {
        fwrite(buf, 1, size, fo);
        fclose(fo);
        printf("[Servidor] [%d] Archivo recibido y cifrado como %s\n", local_port, outname);
    } else {
        perror("fopen salida");
    }

    free(buf);

    const char *ok = "OK\n";
    send_all(cfd, ok, strlen(ok));
    close(cfd);
}

static int make_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    printf("[Servidor] Escuchando puerto %d\n", port);
    return fd;
}

int main(void) {
    int lfd[NPORTS];
    for (int i = 0; i < NPORTS; ++i) {
        lfd[i] = make_listen(PORTS[i]);
        if (lfd[i] < 0) {
            fprintf(stderr, "No pude abrir puerto %d\n", PORTS[i]);
            return 1;
        }
    }

    // bucle principal con select()
    while (1) {
        fd_set rfds; FD_ZERO(&rfds);
        int maxfd = -1;
        for (int i = 0; i < NPORTS; ++i) {
            FD_SET(lfd[i], &rfds);
            if (lfd[i] > maxfd) maxfd = lfd[i];
        }

        int r = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i < NPORTS; ++i) {
            if (FD_ISSET(lfd[i], &rfds)) {
                struct sockaddr_in cli; socklen_t cl = sizeof(cli);
                int cfd = accept(lfd[i], (struct sockaddr*)&cli, &cl);
                if (cfd < 0) { perror("accept"); continue; }
                printf("[*] [SERVER %d] Client %s connected\n",
                       PORTS[i], inet_ntoa(cli.sin_addr));
                handle_client(cfd, PORTS[i]);
            }
        }
    }

    for (int i = 0; i < NPORTS; ++i) close(lfd[i]);
    return 0;
}
