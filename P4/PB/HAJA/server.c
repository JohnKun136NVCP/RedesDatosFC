// Joshua Abel Hurtado Aponte
// server.c — Parte B
// puede escuchar en varios alias (s01..s04) todos en el mismo puerto
// cuando recibe un archivo lo guarda en la carpeta ~/s01, ~/s02, ~/s03 o ~/s04
//
// se puede usar de dos formas:
//   ./server_final 49200
//   ./server_final 49200 s01 s02 s03 s04

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 16
#define LINEA_MAX 512
#define MAX_ESCUCHAS 16

// ip fijas para cada alias 
#define IP_S01 "192.168.64.101"
#define IP_S02 "192.168.64.102"
#define IP_S03 "192.168.64.103"
#define IP_S04 "192.168.64.104"

/* --- lee una línea del socket  --- */
static ssize_t leer_linea(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        buf[n++] = c;
        if (c == '\n') break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

/* --- enviar todo el buffer por el socket --- */
static int enviar_todo(int fd, const char *buf, size_t n) {
    size_t enviado = 0;
    while (enviado < n) {
        ssize_t w = send(fd, buf + enviado, n - enviado, 0);
        if (w <= 0) { if (errno == EINTR) continue; return -1; }
        enviado += (size_t)w;
    }
    return 0;
}

/* --- abre un puerto de datos  --- */
static int abrir_puerto_datos(int *puerto_out) {
    // primero se intenta dejar que el kernel me asigne
    for (int i = 0; i < 64; i++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = 0;
        if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) { close(s); continue; }
        socklen_t alen = sizeof(a);
        if (getsockname(s, (struct sockaddr*)&a, &alen) < 0) { close(s); continue; }
        int p = ntohs(a.sin_port);
        if (p <= 49200) { close(s); continue; }
        if (listen(s, BACKLOG) < 0) { close(s); continue; }
        *puerto_out = p;
        fprintf(stderr, "[DEBUG] puerto datos efímero %d\n", p);
        return s;
    }
    // si no funcionó, se busca manualmente en 50001–51000
    for (int p = 50001; p <= 51000; p++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons((uint16_t)p);
        if (bind(s, (struct sockaddr*)&a, sizeof(a)) == 0 && listen(s, BACKLOG) == 0) {
            *puerto_out = p;
            fprintf(stderr, "[DEBUG] puerto datos fijo %d\n", p);
            return s;
        }
        close(s);
    }
    fprintf(stderr, "[ERROR] no hay puerto datos libre\n");
    return -1;
}

/* --- decide alias  --- */
static const char* alias_por_fd(int fd) {
    struct sockaddr_in local; socklen_t len = sizeof(local);
    if (getsockname(fd, (struct sockaddr*)&local, &len) == 0) {
        char ip[64];
        if (inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip))) {
            if (strcmp(ip, IP_S01) == 0) return "s01";
            if (strcmp(ip, IP_S02) == 0) return "s02";
            if (strcmp(ip, IP_S03) == 0) return "s03";
            if (strcmp(ip, IP_S04) == 0) return "s04";
        }
    }
    return "recv"; // fallback si no coincide
}

/* --- asegurar que exista un directorio --- */
static void asegurar_dir(const char *path) {
    mkdir(path, 0755);
}

/* --- atiende una conexión y recibe un archivo --- */
static void atender_control(int cfd) {
    char linea[LINEA_MAX];
    ssize_t ln = leer_linea(cfd, linea, sizeof(linea));
    if (ln <= 0) return;

    fprintf(stderr, "[DEBUG] cabecera: '%s'\n", linea);

    char nombre_arch[256]; size_t tam = 0;
    if (sscanf(linea, "FILENAME=%255[^;];LEN=%zu", nombre_arch, &tam) != 2) {
        enviar_todo(cfd, "ERROR formato cabecera\n", 23);
        return;
    }

    int puerto_datos = -1;
    int fd_datos = abrir_puerto_datos(&puerto_datos);
    if (fd_datos < 0) {
        enviar_todo(cfd, "ERROR no hay puerto datos\n", 26);
        return;
    }

    char resp[64];
    int n = snprintf(resp, sizeof(resp), "DATA_PORT=%d\n", puerto_datos);
    if (enviar_todo(cfd, resp, (size_t)n) != 0) { close(fd_datos); return; }
    fprintf(stderr, "[DEBUG] DATA_PORT=%d enviado\n", puerto_datos);

    // se espera la conexión real de datos
    struct sockaddr_in da; socklen_t dl = sizeof(da);
    int dfd = accept(fd_datos, (struct sockaddr*)&da, &dl);
    close(fd_datos);
    if (dfd < 0) { perror("[ERROR] accept datos"); return; }

    // se elige en qué carpeta guardar según alias
    const char *alias = alias_por_fd(cfd);
    const char *home  = getenv("HOME"); if (!home) home=".";
    char dir_alias[512]; snprintf(dir_alias, sizeof(dir_alias), "%s/%s", home, alias);
    asegurar_dir(dir_alias);

    char ruta_final[1024]; snprintf(ruta_final, sizeof(ruta_final), "%s/%s", dir_alias, nombre_arch);

    FILE *out = fopen(ruta_final, "wb");
    if (!out) { perror("[ERROR] fopen out"); close(dfd); return; }

    size_t recibido = 0; char buf[4096];
    printf("[SERVER][%s] RECIBIENDO %s (%zu B) -> %s\n", alias, nombre_arch, tam, ruta_final);
    fflush(stdout);
    while (recibido < tam) {
        size_t falta = tam - recibido; if (falta > sizeof(buf)) falta = sizeof(buf);
        ssize_t r = recv(dfd, buf, falta, 0);
        if (r <= 0) break;
        fwrite(buf, 1, (size_t)r, out);
        recibido += (size_t)r;
    }
    fclose(out); close(dfd);

    if (recibido == tam)
        printf("[SERVER][%s] COMPLETADO %s (%zu B)\n", alias, ruta_final, recibido);
    else
        fprintf(stderr, "[SERVER][%s] INCOMPLETO %s (%zu/%zu B)\n", alias, ruta_final, recibido, tam);
    fflush(stdout);
}

/* --- nombre tipo “s01” o ip literal --- */
static int resolver_ip(const char *nombre, struct in_addr *out) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_INET;
    int rc = getaddrinfo(nombre, NULL, &hints, &res);
    if (rc != 0 || !res) return -1;
    struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;
    *out = sin->sin_addr;
    freeaddrinfo(res);
    return 0;
}

/* --- abre un socket de escucha --- */
static int escuchar_en(struct in_addr ip, int puerto) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr = ip; a.sin_port = htons((uint16_t)puerto);
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) { close(s); return -1; }
    if (listen(s, BACKLOG) < 0) { close(s); return -1; }
    return s;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Uso: %s <PUERTO> [s01 s02 s03 s04]\n", argv[0]); return 1; }
    int puerto = atoi(argv[1]);

    int listeners[MAX_ESCUCHAS]; int nls = 0;
    if (argc == 2) {
        // modo simple
        struct in_addr any; any.s_addr = INADDR_ANY;
        int fd = escuchar_en(any, puerto);
        if (fd < 0) return 1;
        listeners[nls++] = fd;
        printf("[+] escuchando en 0.0.0.0:%d (todas las IP locales)\n", puerto);
    } else {
        // modo multi
        for (int i = 2; i < argc && nls < MAX_ESCUCHAS; i++) {
            struct in_addr ip;
            if (resolver_ip(argv[i], &ip) != 0) { fprintf(stderr, "no resolvió %s\n", argv[i]); continue; }
            int fd = escuchar_en(ip, puerto);
            if (fd >= 0) {
                listeners[nls++] = fd;
                char ipstr[32]; inet_ntop(AF_INET, &ip, ipstr, sizeof(ipstr));
                printf("[+] escuchando en %s:%d (%s)\n", ipstr, puerto, argv[i]);
            }
        }
        if (nls == 0) { fprintf(stderr, "ningún listener válido\n"); return 1; }
    }

    // select() para vigilar todos los sockets
    for (;;) {
        fd_set rfds; FD_ZERO(&rfds);
        int mx = -1;
        for (int i = 0; i < nls; i++) { FD_SET(listeners[i], &rfds); if (listeners[i] > mx) mx = listeners[i]; }
        int rc = select(mx + 1, &rfds, NULL, NULL, NULL);
        if (rc < 0) { if (errno == EINTR) continue; perror("select"); break; }

        for (int i = 0; i < nls; i++) if (FD_ISSET(listeners[i], &rfds)) {
            struct sockaddr_in cli; socklen_t cl = sizeof(cli);
            int cfd = accept(listeners[i], (struct sockaddr*)&cli, &cl);
            if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }
            atender_control(cfd);
            close(cfd);
        }
    }
    for (int i = 0; i < nls; i++) close(listeners[i]);
    return 0;
}
//finn
