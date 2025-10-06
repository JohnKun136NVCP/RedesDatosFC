// client.c
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define CTRL_PORT   49200

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : ".";
}

static void log_line(const char *tag, int port, const char *estado) {
    char dir[512], path[600];
    snprintf(dir, sizeof(dir), "%s/%s", home_dir(), tag);
    (void)mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/status.log", dir);
    time_t now = time(NULL);
    struct tm tm; localtime_r(&now, &tm);
    char ts[32]; strftime(ts, sizeof(ts), "%F %T", &tm);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s %s:%d %s\n", ts, tag, port, estado);
    fclose(f);
}

static int tcp_connect_host(const char *host_or_ip, int port) {
    struct addrinfo hints, *res, *p;
    char portstr[16];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(portstr, sizeof(portstr), "%d", port);
    int rc = getaddrinfo(host_or_ip, portstr, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s:%s): %s\n",
                host_or_ip, portstr, gai_strerror(rc));
        return -1;
    }

    int s = -1;
    for (p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        close(s);
        s = -1;
    }
    freeaddrinfo(res);
    return s;
}

static int pedir_newport(const char *host) {
    int s = tcp_connect_host(host, CTRL_PORT);
    if (s < 0) return -1;

    const char *hello = "HELLO\n";
    send(s, hello, strlen(hello), 0);
    char buf[128];
    ssize_t r = recv(s, buf, sizeof(buf) - 1, 0);
    close(s);
    if (r <= 0) return -1;
    buf[r] = '\0';
    int port = -1;
    if (sscanf(buf, "NEWPORT=%d", &port) != 1) return -1;
    return port;
}

static char *leerArchivo(const char *ruta, size_t *n_out) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror("open archivo"); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';
    *n_out = n;
    return buf;
}

static int mandar(const char *host, const char *tag,
                  int puertoServer, int portoObjetivo, int shift,
                  const char *msg, size_t len) {
    int s = tcp_connect_host(host, puertoServer);
    if (s < 0) return -1;

    char header[128];
    int hlen = snprintf(header, sizeof(header),
                        "PORTO=%d;SHIFT=%d;LEN=%zu\n",
                        portoObjetivo, shift, len);

    log_line(tag, puertoServer, "RECIBIENDO");

    send(s, header, hlen, 0);
    send(s, msg, len, 0);

    char buf[BUFFER_SIZE + 1];
    ssize_t r = recv(s, buf, BUFFER_SIZE, 0);
    if (r > 0) {
        buf[r] = '\0';
        printf("Respuesta: %s\n", buf);
    }

    close(s);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <HOST_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <archivo>\n", argv[0]);
        return 1;
    }

    const char *nombre_srv = argv[1];
    int portoObjetivo      = atoi(argv[2]);
    int shift              = atoi(argv[3]);
    const char *archivo    = argv[4];

    log_line(nombre_srv, portoObjetivo, "ESPERA");
    int np = pedir_newport(nombre_srv);
    if (np <= 0) {
        log_line(nombre_srv, portoObjetivo, "ERROR_NO_PORT");
        return 1;
    }
    portoObjetivo = np;

    size_t nlen = 0;
    char *msg = leerArchivo(archivo, &nlen);
    if (!msg) return 1;

    int rc = mandar(nombre_srv, nombre_srv, portoObjetivo, portoObjetivo, shift, msg, nlen);
    log_line(nombre_srv, portoObjetivo, (rc == 0) ? "OK" : "FAIL");
    free(msg);
    return rc;
}