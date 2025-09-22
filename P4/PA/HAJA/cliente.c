/*
  Joshua Abel Huetado Aponte
  Práctica 4 (Parte A)
  Los estados se imprimen en pantalla y también se guardan en:
  $HOME/<nombre-servidor>/status.log  
*/

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
#define CTRL_HOST_DEFAULT "srv"

// nos regresa $HOME o "." si es que no existe
static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : ".";
}

// línea de log "YYYY-mm-dd HH:MM:SS tag:puerto estado"
// en $HOME/<tag>/status.log 
static void log_line(const char *tag, int port, const char *estado) {
    char dir[512], path[600];
    snprintf(dir,  sizeof(dir), "%s/%s", home_dir(), tag);
    (void)mkdir(dir, 0755);  // si ya existe, ignoramos esto 
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
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

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

    if (s < 0) perror("connect");
    return s;
}

// Pregunta al broker en y devuelve el newport
static int pedir_newport(const char *ctrl_host) {
    int s = tcp_connect_host(ctrl_host, CTRL_PORT);
    if (s < 0) return -1;

    const char *hello = "HELLO\n";
    if (send(s, hello, strlen(hello), 0) < 0) {
        perror("send HELLO");
        close(s);
        return -1;
    }
    char buf[128];
    ssize_t r = recv(s, buf, sizeof(buf) - 1, 0);
    close(s);
    if (r <= 0) return -1;
    buf[r] = '\0';
    int port = -1;
    if (sscanf(buf, "NEWPORT=%d", &port) != 1) return -1;
    return port;
}

// Abre archivo y se queda solo con letras, tambien nos ayuda con acentos (hablamos español jaj)
static char *leerSoloLetras(const char *ruta, size_t *n_out) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror("open archivo"); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *raw = malloc((size_t)sz + 1);
    if (!raw) { fclose(f); return NULL; }

    size_t n = fread(raw, 1, (size_t)sz, f);
    fclose(f);
    raw[n] = '\0';

    char *clean = malloc(n + 1);
    if (!clean) { free(raw); return NULL; }

    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)raw[i];

        // vocales acentuadas y ñ/Ñ 
        if (c == 0xC3 && i + 1 < n) {
            unsigned char d = (unsigned char)raw[i + 1];
            if (d==0xA1||d==0x81) { c='a'; i++; }  // á/Á
            else if (d==0xA9||d==0x89) { c='e'; i++; }
            else if (d==0xAD||d==0x8D) { c='i'; i++; }
            else if (d==0xB3||d==0x93) { c='o'; i++; }
            else if (d==0xBA||d==0x9A) { c='u'; i++; }
            else if (d==0xB1||d==0x91) { c='n'; i++; }  // ñ/Ñ
        }

        if (isalpha(c)) clean[j++] = (char)c;
    }
    clean[j] = '\0';

    *n_out = j;
    free(raw);
    return clean;
}

// Conecta a puerto de datos, envía la cabecera y cuerpo, muestra respuesta.
static int mandar(const char *conn_host, const char *logdir,
                  int puertoServer, int portoObjetivo, int shift,
                  const char *msg, size_t len) {
    int s = tcp_connect_host(conn_host, puertoServer);
    if (s < 0) return -1;

    // Cabecera 
    char header[128];
    int hlen = snprintf(header, sizeof(header),
                        "PORTO=%d;SHIFT=%d;LEN=%zu\n",
                        portoObjetivo, shift, len);
    if (hlen <= 0 || hlen >= (int)sizeof(header)) {
        fprintf(stderr, "header invalido\n");
        close(s);
        return -1;
    }

    puts("RECIBIENDO");
    log_line(logdir, puertoServer, "RECIBIENDO");

    if (send(s, header, (size_t)hlen, 0) != hlen) {
        perror("send header");
        close(s);
        return -1;
    }

    // Cuerpo
    size_t sent = 0;
    while (sent < len) {
        ssize_t w = send(s, msg + sent, len - sent, 0);
        if (w <= 0) { perror("send body"); close(s); return -1; }
        sent += (size_t)w;
    }

    // Respuesta de server
    printf("\n--- Respuesta de %s:%d (PORTO=%d) ---\n",
           conn_host, puertoServer, portoObjetivo);
    char buf[BUFFER_SIZE + 1];
    int recibimos_algo = 0;
    for (;;) {
        ssize_t r = recv(s, buf, BUFFER_SIZE, 0);
        if (r <= 0) break;
        recibimos_algo = 1;
        buf[r] = '\0';
        fputs(buf, stdout);
    }
    printf("\n");
    close(s);
    if (recibimos_algo) log_line(logdir, puertoServer, "TRANSMITIENDO");
    return recibimos_algo ? 0 : -1;
}

// main
int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <NOMBRE_SERVIDOR|HOST/IP> <PUERTO_OBJETIVO> <SHIFT> <archivo>\n", argv[0]);
        printf("Ejemplo:  %s s01 49200 3 texto.txt   (log en ~/s01/status.log; broker 'srv')\n", argv[0]);
        return 1;
    }

    const char *nombre_srv = argv[1];    
    int portoObjetivo       = atoi(argv[2]);
    int shift               = atoi(argv[3]);
    const char *archivo     = argv[4];

    const char *ctrl_host   = CTRL_HOST_DEFAULT;
    int es_s01_s02          = (!strcmp(nombre_srv, "s01") || !strcmp(nombre_srv, "s02"));
    const char *logdir      = nombre_srv;
    const char *conn_host   = es_s01_s02 ? ctrl_host : nombre_srv;

    // puerto de control, primero pedimos el NEWPORT
    if (portoObjetivo == CTRL_PORT) {
        puts("ESPERA");
        log_line(logdir, CTRL_PORT, "ESPERA");

        int np = pedir_newport(conn_host);
        if (np <= 0) {
            fprintf(stderr, "No se obtuvo NEWPORT\n");
            log_line(logdir, CTRL_PORT, "ERROR_NO_PORT");
            return 1;
        }
        portoObjetivo = np;
        printf("NEWPORT=%d\n", portoObjetivo);
    }

    // Preparo el mensaje  
    size_t nlen = 0;
    char *msg = leerSoloLetras(archivo, &nlen);
    if (!msg || nlen == 0) {
        fprintf(stderr, "no se pudo leer/construir el mensaje desde '%s'\n", archivo);
        free(msg);
        return 1;
    }

    // enviamos al puerto de datos asignado
    int rc = mandar(conn_host, logdir, portoObjetivo, portoObjetivo, shift, msg, nlen);

    // Resultado 
    log_line(logdir, portoObjetivo, (rc == 0) ? "OK" : "FAIL");
    free(msg);
    return (rc == 0) ? 0 : 1;
}
//finn