// Joshua Abel Hurtado Aponte
// server.c — Práctica 4 (Parte A)
// Guarda archivos en ~/s01, ~/s02

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG   16
#define LINEA_MAX 512

#define IP_S01 "192.168.64.101"
#define IP_S02 "192.168.64.102"
#define IP_S03 "192.168.64.103"
#define IP_S04 "192.168.64.104"

/* --- lee una línea del socket --- */
static ssize_t leer_linea(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;                        // se cerró del otro lado
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        buf[n++] = c;
        if (c == '\n') break;                     // fin 
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

/* --- manda todo el buffer por el socket  --- */
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

/* --- abre un puerto de datos --- */
static int abrir_puerto_datos(int *puerto_out) {
    for (int i = 0; i < 64; i++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;

        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family      = AF_INET;
        a.sin_addr.s_addr = INADDR_ANY;
        a.sin_port        = 0;               // que el kernel asigne

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

    // rango fijo 50001–51000
    for (int p = 50001; p <= 51000; p++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;

        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family      = AF_INET;
        a.sin_addr.s_addr = INADDR_ANY;
        a.sin_port        = htons((uint16_t)p);

        if (bind(s, (struct sockaddr*)&a, sizeof(a)) == 0 && listen(s, BACKLOG) == 0) {
            *puerto_out = p;
            fprintf(stderr, "[DEBUG] puerto datos fijo %d\n", p);
            return s;
        }
        close(s);
    }

    fprintf(stderr, "[ERROR] no hay puerto de datos libre\n");
    return -1;
}

/* --- devuelve "s01|s02ol */
static const char* alias_por_fd(int fd) {
    struct sockaddr_in local; socklen_t len = sizeof(local);
    if (getsockname(fd, (struct sockaddr*)&local, &len) == 0) {
        char ip[64];
        if (inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip))) {
            if (strcmp(ip, IP_S01) == 0) return "s01";
            if (strcmp(ip, IP_S02) == 0) return "s02";
        }
    }
    return "recv"; // por si no funciona con ningún alias 
}

/* --- crea directorio si no existe  --- */
static void asegurar_dir(const char *path) {
    mkdir(path, 0755);
}

/* --- conexión de control --- */
static void atender_control(int cfd) {
    char linea[LINEA_MAX];
    ssize_t ln = leer_linea(cfd, linea, sizeof(linea));
    if (ln <= 0) return;

    fprintf(stderr, "[DEBUG] cabecera: '%s'\n", linea);

    char nombre_arch[256];
    size_t tam = 0;
    if (sscanf(linea, "FILENAME=%255[^;];LEN=%zu", nombre_arch, &tam) != 2) {
        const char *bad = "ERROR formato cabecera\n";
        enviar_todo(cfd, bad, strlen(bad));
        return;
    }

    // se abre el puerto de datos y se le dice al cliente
    int puerto_datos = -1;
    int fd_datos = abrir_puerto_datos(&puerto_datos);
    if (fd_datos < 0) {
        const char *err = "ERROR no hay puerto datos\n";
        enviar_todo(cfd, err, strlen(err));
        return;
    }

    char resp[64];
    int n = snprintf(resp, sizeof(resp), "DATA_PORT=%d\n", puerto_datos);
    if (enviar_todo(cfd, resp, (size_t)n) != 0) {
        close(fd_datos);
        return;
    }
    fprintf(stderr, "[DEBUG] DATA_PORT=%d enviado\n", puerto_datos);

    // se espera la conexión real 
    struct sockaddr_in da; socklen_t dl = sizeof(da);
    int dfd = accept(fd_datos, (struct sockaddr*)&da, &dl);
    close(fd_datos);
    if (dfd < 0) { perror("[ERROR] accept datos"); return; }

    // decido carpeta por alias de la IP 
    const char *alias = alias_por_fd(cfd);
    const char *home  = getenv("HOME"); if (!home) home = ".";
    char dir_alias[512]; snprintf(dir_alias, sizeof(dir_alias), "%s/%s", home, alias);
    asegurar_dir(dir_alias);

    char ruta_final[1024]; snprintf(ruta_final, sizeof(ruta_final), "%s/%s", dir_alias, nombre_arch);

    // recibo el archivo y lo dejo en la carpeta 
    FILE *out = fopen(ruta_final, "wb");
    if (!out) { perror("[ERROR] fopen out"); close(dfd); return; }

    size_t recibido = 0;
    char buf[4096];

    printf("[SERVER][%s] RECIBIENDO %s (%zu bytes) -> %s\n", alias, nombre_arch, tam, ruta_final);
    fflush(stdout);

    while (recibido < tam) {
        size_t falta = tam - recibido; if (falta > sizeof(buf)) falta = sizeof(buf);
        ssize_t r = recv(dfd, buf, falta, 0);
        if (r <= 0) break;
        fwrite(buf, 1, (size_t)r, out);
        recibido += (size_t)r;
    }
    fclose(out);
    close(dfd);

    if (recibido == tam) {
        printf("[SERVER][%s] COMPLETADO %s (%zu bytes)\n", alias, ruta_final, recibido);
    } else {
        fprintf(stderr, "[SERVER][%s] INCOMPLETO %s (%zu/%zu bytes)\n",
                alias, ruta_final, recibido, tam);
    }
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO_CONTROL>\n", argv[0]);
        return 1;
    }
    int puerto = atoi(argv[1]);

    // listener del puerto de control
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int yes = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port        = htons((uint16_t)puerto);

    if (bind(sfd, (struct sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); close(sfd); return 1; }
    if (listen(sfd, BACKLOG) < 0) { perror("listen"); close(sfd); return 1; }

    printf("[+] Server (control) escuchando en %d\n", puerto);
    fflush(stdout);

    // se atienden conexiones de control y se reciben archivos
    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        atender_control(cfd);
        close(cfd);
    }

    close(sfd);
    return 0;
}
//finnnn