// server.c — Práctica V 
// Joshua Abel Hurtado Aponte
// Guarda los archivos en ~/s01, ~/s02, ~/s03 o ~/s04.
// ./p5_server 49200 s01 s02 s03 s04

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG     16
#define LINEA_MAX   512
#define MAX_ALIAS   16

// IPs usadas en mi prácticas (
#define IP_S01 "192.168.64.101"
#define IP_S02 "192.168.64.102"
#define IP_S03 "192.168.64.103"
#define IP_S04 "192.168.64.104"

// Utilidades 
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

static int enviar_todo(int fd, const char *buf, size_t n) {
    size_t enviado = 0;
    while (enviado < n) {
        ssize_t w = send(fd, buf + enviado, n - enviado, 0);
        if (w <= 0) { if (errno == EINTR) continue; return -1; }
        enviado += (size_t)w;
    }
    return 0;
}

// puerto de datos >49200, si no se busca 50001–51000
static int abrir_puerto_datos(int *puerto_out) {
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
        return s;
    }
    for (int p = 50001; p <= 51000; p++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons((uint16_t)p);
        if (bind(s, (struct sockaddr*)&a, sizeof(a)) == 0 && listen(s, BACKLOG) == 0) {
            *puerto_out = p;
            return s;
        }
        close(s);
    }
    fprintf(stderr, "[ERROR] no hay puerto de datos libre\n");
    return -1;
}

// mapea la IP local del socket 
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
    return "recv";
}

static void asegurar_dir(const char *path) {
    mkdir(path, 0755); 
}

// Estructuras para cada alias
typedef struct ConnNode {
    int cfd;
    struct ConnNode *sig;
} ConnNode;

typedef struct {
    const char *nombre;     // s01...s04
    char ipstr[32];         // ip del alias
    int lfd;                // listener 
    ConnNode *q_head;       
    ConnNode *q_tail;
    pthread_mutex_t mtx;    // lock por alias para su cola
} Alias;

static Alias ALIAS[MAX_ALIAS];
static int NUM_ALIAS = 0;

// estado global del planificador
static pthread_t th_aceptador;
static pthread_t th_planificador;
static volatile int corriendo = 1;   
static int indice_turno = 0;         // índice actual

// Recibir un solo archivo
static void recibir_archivo_de_control(int cfd) {
    char linea[LINEA_MAX];
    ssize_t ln = leer_linea(cfd, linea, sizeof(linea));
    if (ln <= 0) return;
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
    if (enviar_todo(cfd, resp, (size_t)n) != 0) {
        close(fd_datos);
        return;
    }

    //podemos aceptar la conexión de datos
    struct sockaddr_in da; socklen_t dl = sizeof(da);
    int dfd = accept(fd_datos, (struct sockaddr*)&da, &dl);
    close(fd_datos);
    if (dfd < 0) {
        perror("[ERROR] accept datos");
        return;
    }

    // Carpeta según IP por la que llegó
    const char *alias = alias_por_fd(cfd);
    const char *home = getenv("HOME"); if (!home) home = ".";
    char dir_alias[512]; snprintf(dir_alias, sizeof(dir_alias), "%s/%s", home, alias);
    asegurar_dir(dir_alias);

    char ruta_final[1024]; snprintf(ruta_final, sizeof(ruta_final), "%s/%s", dir_alias, nombre_arch);

    // Recibir 
    FILE *out = fopen(ruta_final, "wb");
    if (!out) { perror("[ERROR] fopen out"); close(dfd); return; }

    size_t recibido = 0; char buf[4096];
    printf("[RR][%s] RECIBIENDO %s (%zu B) -> %s\n", alias, nombre_arch, tam, ruta_final);
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
        printf("[RR][%s] COMPLETADO %s (%zu B)\n", alias, ruta_final, recibido);
    } else {
        fprintf(stderr, "[RR][%s] INCOMPLETO %s (%zu/%zu B)\n", alias, ruta_final, recibido, tam);
    }
    fflush(stdout);
}

// Aceptador
static void *hilo_aceptador_fn(void *arg) {
    (void)arg;
    while (corriendo) {
        fd_set rfds; FD_ZERO(&rfds);
        int mx = -1;
        for (int i = 0; i < NUM_ALIAS; i++) {
            FD_SET(ALIAS[i].lfd, &rfds);
            if (ALIAS[i].lfd > mx) mx = ALIAS[i].lfd;
        }
        int rc = select(mx + 1, &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("[ERROR] select aceptador");
            break;
        }
        for (int i = 0; i < NUM_ALIAS; i++) if (FD_ISSET(ALIAS[i].lfd, &rfds)) {
            struct sockaddr_in cli; socklen_t cl = sizeof(cli);
            int cfd = accept(ALIAS[i].lfd, (struct sockaddr*)&cli, &cl);
            if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

            //conexión en el alias correspondiente
            ConnNode *nd = (ConnNode*)calloc(1, sizeof(ConnNode));
            if (!nd) { close(cfd); continue; }
            nd->cfd = cfd;

            pthread_mutex_lock(&ALIAS[i].mtx);
            if (!ALIAS[i].q_tail) {
                ALIAS[i].q_head = ALIAS[i].q_tail = nd;
            } else {
                ALIAS[i].q_tail->sig = nd;
                ALIAS[i].q_tail = nd;
            }
            pthread_mutex_unlock(&ALIAS[i].mtx);
        }
    }
    return NULL;
}

// Planificador Round Robin 
static void *hilo_planificador_fn(void *arg) {
    (void)arg;
    // Si hay trabajo en el alias, COMPLETO y se avanza
    while (corriendo) {
        int hizo_algo = 0;
        // alias 
        int i = indice_turno % NUM_ALIAS;
        // hay conexión en cola para este alias?
        pthread_mutex_lock(&ALIAS[i].mtx);
        ConnNode *nd = ALIAS[i].q_head;
        if (nd) {
            // sacamos de la cola
            ALIAS[i].q_head = nd->sig;
            if (!ALIAS[i].q_head) ALIAS[i].q_tail = NULL;
        }
        pthread_mutex_unlock(&ALIAS[i].mtx);

        if (nd) {
            printf("[RR] TURNO de %s — atendiendo una conexión\n", ALIAS[i].nombre);
            fflush(stdout);
            // Atiende control 
            recibir_archivo_de_control(nd->cfd);
            close(nd->cfd);
            free(nd);
            hizo_algo = 1;
        }

        // Avanza turno siempre 
        indice_turno = (indice_turno + 1) % NUM_ALIAS;

        // evita girar sin importancia
        if (!hizo_algo) {
            usleep(1000 * 10); 
        }
    }
    return NULL;
}

// resolver y escuchar
static int resolver_ipv4(const char *nombre, struct in_addr *out) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(nombre, NULL, &hints, &res) != 0 || !res) return -1;
    struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;
    *out = sin->sin_addr;
    freeaddrinfo(res);
    return 0;
}

static int escuchar_en_ip(struct in_addr ip, int puerto) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr = ip; a.sin_port = htons((uint16_t)puerto);
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); close(s); return -1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); close(s); return -1; }
    return s;
}

// MAIN
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <PUERTO> [s01 s02 s03 s04]\n", argv[0]);
        return 1;
    }
    int puerto = atoi(argv[1]);

    if (argc == 2) {
        // Modo simple
        struct in_addr any; any.s_addr = INADDR_ANY;
        int fd = escuchar_en_ip(any, puerto);
        if (fd < 0) return 1;

        ALIAS[0].nombre = "any";
        strcpy(ALIAS[0].ipstr, "0.0.0.0");
        ALIAS[0].lfd = fd;
        ALIAS[0].q_head = ALIAS[0].q_tail = NULL;
        pthread_mutex_init(&ALIAS[0].mtx, NULL);
        NUM_ALIAS = 1;

        printf("[+] Escuchando en 0.0.0.0:%d\n", puerto);
    } else {
        // Modo multialias (s01..s04)
        for (int i = 2; i < argc && NUM_ALIAS < MAX_ALIAS; i++) {
            struct in_addr ip;
            if (resolver_ipv4(argv[i], &ip) != 0) {
                fprintf(stderr, "[WARN] no resolvió %s (lo salto)\n", argv[i]);
                continue;
            }
            int fd = escuchar_en_ip(ip, puerto);
            if (fd < 0) { fprintf(stderr, "[WARN] no pude escuchar %s\n", argv[i]); continue; }

            ALIAS[NUM_ALIAS].nombre = argv[i];
            inet_ntop(AF_INET, &ip, ALIAS[NUM_ALIAS].ipstr, sizeof(ALIAS[NUM_ALIAS].ipstr));
            ALIAS[NUM_ALIAS].lfd = fd;
            ALIAS[NUM_ALIAS].q_head = ALIAS[NUM_ALIAS].q_tail = NULL;
            pthread_mutex_init(&ALIAS[NUM_ALIAS].mtx, NULL);

            printf("[+] Escuchando en %s:%d (%s)\n", ALIAS[NUM_ALIAS].ipstr, puerto, argv[i]);
            NUM_ALIAS++;
        }
        if (NUM_ALIAS == 0) { fprintf(stderr, "Sin listeners válidos\n"); return 1; }
    }

    // hilos
    if (pthread_create(&th_aceptador, NULL, hilo_aceptador_fn, NULL) != 0) {
        perror("pthread_create aceptador"); return 1;
    }
    if (pthread_create(&th_planificador, NULL, hilo_planificador_fn, NULL) != 0) {
        perror("pthread_create planificador"); return 1;
    }

    // dejamos que los hilos trabajen
    pthread_join(th_aceptador, NULL);
    pthread_join(th_planificador, NULL);

    // limpieza opcional
    for (int i = 0; i < NUM_ALIAS; i++) close(ALIAS[i].lfd);
    return 0;
}

//finnn

