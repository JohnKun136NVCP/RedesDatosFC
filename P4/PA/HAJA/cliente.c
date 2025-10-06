// Joshua Abel Hurtado Aponte
// cliente.c — Práctica 4 (Parte A)

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- conectar a host:puerto por TCP  --- */
static int conectar_tcp(const char *host, int puerto) {
    char pstr[16];
    snprintf(pstr, sizeof(pstr), "%d", puerto);

    struct addrinfo hints, *res = NULL, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, pstr, &hints, &res) != 0) return -1;

    int s = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0) continue;
        if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(s); s = -1;
    }
    freeaddrinfo(res);
    return s; // -1 
}

/* --- timestamp corto para logs  --- */
static void hora_actual(char *out, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, cap, "%Y-%m-%d %H:%M:%S", &tm);
}

/* --- escribir línea status_<host>.log  --- */
static void escribir_linea_log(const char *archivo_log, const char *linea) {
    FILE *f = fopen(archivo_log, "a");
    if (!f) return;
    char ts[32];
    hora_actual(ts, sizeof(ts));
    fprintf(f, "%s %s\n", ts, linea);
    fclose(f);
}

/* --- log por destino  --- */
static void log_estado_destino(const char *destino, const char *estado) {
    char fn[256];
    snprintf(fn, sizeof(fn), "status_%s.log", destino);
    escribir_linea_log(fn, estado);
}

/* --- leer línea "DATA_PORT=NNNN" del socket de control --- */
static int leer_puerto_datos(int fd_control) {
    char linea[128];
    size_t n = 0;
    while (n + 1 < sizeof(linea)) {
        char c;
        ssize_t r = recv(fd_control, &c, 1, 0);
        if (r == 0) break;        // server cerró
        if (r < 0) return -1;     // error
        linea[n++] = c;
        if (c == '\n') break;     // fin 
    }
    linea[n] = '\0';

    int puerto = -1;
    if (sscanf(linea, "DATA_PORT=%d", &puerto) == 1) return puerto;

    return -1;
}

/* --- mandar todo un buffer por el socket  --- */
static int enviar_todo(int fd, const char *buf, size_t n) {
    size_t enviado = 0;
    while (enviado < n) {
        ssize_t w = send(fd, buf + enviado, n - enviado, 0);
        if (w <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        enviado += (size_t)w;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <SERVIDOR> <PUERTO_CONTROL> <archivo>\n", argv[0]);
        return 1;
    }

    const char *servidor   = argv[1];
    int puerto_control     = atoi(argv[2]);
    const char *ruta       = argv[3];

    // leer el archivo completo 
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); fprintf(stderr, "tamaño inválido\n"); return 1; }

    char *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); fprintf(stderr, "sin memoria\n"); return 1; }
    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (nread != (size_t)sz) {
        free(buf);
        fprintf(stderr, "fread parcial\n");
        return 1;
    }

    // CONTROL 
    log_estado_destino(servidor, "ESPERA");
    int cfd = conectar_tcp(servidor, puerto_control);
    if (cfd < 0) { perror("connect control"); free(buf); return 1; }

    // cabecera con nombre y tamaño
    const char *nombre_arch = strrchr(ruta, '/');
    nombre_arch = nombre_arch ? nombre_arch + 1 : ruta;

    char cabecera[512];
    int hlen = snprintf(cabecera, sizeof(cabecera),
                        "FILENAME=%s;LEN=%zu\n", nombre_arch, nread);
    if (enviar_todo(cfd, cabecera, (size_t)hlen) != 0) {
        perror("send header");
        close(cfd);
        free(buf);
        return 1;
    }

    // el server responde con el puerto de datos
    int puerto_datos = leer_puerto_datos(cfd);
    close(cfd);
    if (puerto_datos <= 0) {
        fprintf(stderr, "ERROR: servidor no devolvió DATA_PORT\n");
        free(buf);
        return 1;
    }

    //  DATOS 
    log_estado_destino(servidor, "TRANSMITIENDO");
    int dfd = conectar_tcp(servidor, puerto_datos);
    if (dfd < 0) { perror("connect data"); free(buf); return 1; }

    if (enviar_todo(dfd, buf, nread) != 0) {
        perror("send data");
        close(dfd);
        free(buf);
        return 1;
    }
    close(dfd);
    free(buf);

    log_estado_destino(servidor, "COMPLETADO");
    printf("[CLIENTE] Enviado '%s' a %s:%d (datos en %d)\n",
           nombre_arch, servidor, puerto_control, puerto_datos);
    return 0;
}
//finnnn
