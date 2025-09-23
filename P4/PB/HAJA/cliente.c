// Joshua Abel Hurtado Aponte
// cliente.c — Parte B
// con --status, el “ID” se conecta a los otros tres 
// modo 1: ./clienteC <SERVIDOR> <PUERTO_CONTROL> <archivo>
// modo 2: ./clienteC --status <ID:s01|s02|s03|s04> <PUERTO_CONTROL> <archivo>


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

/* --- conectar a host puerto por TCP --- */
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
    return s; // -1 si no 
}

/* --- timestamp corto para logs --- */
static void hora_actual(char *out, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, cap, "%Y-%m-%d %H:%M:%S", &tm);
}

/* --- escribir una línea en un archivo de log --- */
static void escribir_linea_log(const char *archivo_log, const char *linea) {
    FILE *f = fopen(archivo_log, "a");
    if (!f) return;
    char ts[32];
    hora_actual(ts, sizeof(ts));
    fprintf(f, "%s %s\n", ts, linea);
    fclose(f);
}

/* --- log por destino --- */
static void log_estado_destino(const char *destino, const char *estado) {
    char fn[256];
    snprintf(fn, sizeof(fn), "status_%s.log", destino);
    escribir_linea_log(fn, estado);
}

/* --- log del “ID” cuando compila en modo --status --- */
static void log_estado_id(const char *id, const char *estado, const char *destino) {
    if (!id) return;
    char fn[256];
    snprintf(fn, sizeof(fn), "status_%s.log", id);
    char linea[256];
    snprintf(linea, sizeof(linea), "%s -> %s", estado, destino ? destino : "-");
    escribir_linea_log(fn, linea);
}
/* --- leer línea con DATA_PORT=NNNN desde el socket de control --- */
static int leer_puerto_datos(int fd_control) {
    char linea[128];
    size_t n = 0;
    while (n + 1 < sizeof(linea)) {
        char c;
        ssize_t r = recv(fd_control, &c, 1, 0);
        if (r == 0) break;        // se cerró del otro lado
        if (r < 0) return -1;     // error 
        linea[n++] = c;
        if (c == '\n') break;     // fin de línea
    }
    linea[n] = '\0';

    int puerto = -1;
    if (sscanf(linea, "DATA_PORT=%d", &puerto) == 1) return puerto;

    fprintf(stderr, "[DEBUG] Respuesta control rara: '%s'\n", linea);
    return -1;
}

/* --- mandar un buffer por el socket --- */
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

/* --- rutina principal de envío --- */
static int enviar_a_servidor(const char *servidor, int puerto_control,
                             const char *nombre_archivo,
                             const char *buf, size_t nbytes,
                             const char *id_para_log /* puede ser NULL */) {
    //  abro conexión y aviso que quiero mandar
    log_estado_destino(servidor, "ESPERA");
    if (id_para_log) log_estado_id(id_para_log, "ESPERA", servidor);

    int cfd = conectar_tcp(servidor, puerto_control);
    if (cfd < 0) { perror("connect control"); return -1; }

    char cabecera[512];
    int hlen = snprintf(cabecera, sizeof(cabecera),
                        "FILENAME=%s;LEN=%zu\n", nombre_archivo, nbytes);
    if (enviar_todo(cfd, cabecera, (size_t)hlen) != 0) {
        perror("send header");
        close(cfd);
        return -1;
    }

    // server contesta con DATA_PORT=NNNN
    int puerto_datos = leer_puerto_datos(cfd);
    close(cfd);
    if (puerto_datos <= 0) {
        fprintf(stderr, "ERROR: el servidor no devolvió DATA_PORT\n");
        return -1;
    }

    // abro el canal de datos y mando el contenido
    log_estado_destino(servidor, "TRANSMITIENDO");
    if (id_para_log) log_estado_id(id_para_log, "TRANSMITIENDO", servidor);

    int dfd = conectar_tcp(servidor, puerto_datos);
    if (dfd < 0) { perror("connect data"); return -1; }

    if (enviar_todo(dfd, buf, nbytes) != 0) {
        perror("send data");
        close(dfd);
        return -1;
    }
    close(dfd);

    log_estado_destino(servidor, "COMPLETADO");
    if (id_para_log) log_estado_id(id_para_log, "COMPLETADO", servidor);

    printf("[CLIENTE] Enviado '%s' a %s:%d (datos en %d)\n",
           nombre_archivo, servidor, puerto_control, puerto_datos);
    return 0;
}

int main(int argc, char **argv) {
    // modo “status” se conecta a los otros tres
    if (argc == 5 && strcmp(argv[1], "--status") == 0) {
        const char *id   = argv[2];               // s01|s02|s03|s04
        int puerto_ctrl  = atoi(argv[3]);
        const char *ruta = argv[4];

        // leo el archivo entero una sola vez
        FILE *f = fopen(ruta, "rb");
        if (!f) { perror("fopen"); return 1; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 0) { fclose(f); fprintf(stderr, "tamaño inválido\n"); return 1; }

        char *buf = malloc((size_t)sz);
        if (!buf) { fclose(f); fprintf(stderr, "sin memoria\n"); return 1; }

        size_t leidos = fread(buf, 1, (size_t)sz, f);
        fclose(f);
        if (leidos != (size_t)sz) {
            free(buf);
            fprintf(stderr, "fread parcial\n");
            return 1;
        }

        const char *nombre = strrchr(ruta, '/');
        nombre = nombre ? nombre + 1 : ruta;

        const char *todos[4] = {"s01","s02","s03","s04"};
        for (int i = 0; i < 4; i++) {
            if (strcmp(todos[i], id) == 0) continue; // no se manda a si mismo
            (void)enviar_a_servidor(todos[i], puerto_ctrl, nombre, buf, leidos, id);
        }

        free(buf);
        return 0;
    }

    // modo simple:  
    if (argc != 4) {
        fprintf(stderr,
            "Uso:\n"
            "  %s <SERVIDOR> <PUERTO_CONTROL> <archivo>\n"
            "  %s --status <ID:s01|s02|s03|s04> <PUERTO_CONTROL> <archivo>\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *servidor   = argv[1];
    int puerto_ctrl        = atoi(argv[2]);
    const char *ruta       = argv[3];

    FILE *f = fopen(ruta, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); fprintf(stderr, "tamaño inválido\n"); return 1; }

    char *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); fprintf(stderr, "sin memoria\n"); return 1; }

    size_t leidos = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (leidos != (size_t)sz) {
        free(buf);
        fprintf(stderr, "fread parcial\n");
        return 1;
    }

    const char *nombre = strrchr(ruta, '/');
    nombre = nombre ? nombre + 1 : ruta;

    int rc = enviar_a_servidor(servidor, puerto_ctrl, nombre, buf, leidos, NULL);
    free(buf);
    return rc == 0 ? 0 : 1;
}
//finnnn
