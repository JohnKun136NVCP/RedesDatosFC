// serverP5.c
// Uso:
//   gcc -Wall -O2 server_rr.c -o server_rr -lpthread
//   ./server_rr 49200 s01 s02 s03 s04

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BACKLOG 64
#define BUF     4096
#define MAXL    16         // máximo de alias/listeners

typedef struct ConnNode {
    int fd;
    struct sockaddr_in peer;
    struct ConnNode *next;
} ConnNode;

typedef struct {
    int              fd;                         // socket listen
    char             name[16];                   // "s01"
    char             ip[INET_ADDRSTRLEN];        // ip resuelta
    ConnNode        *q_head;
    ConnNode        *q_tail;
} Listener;

static Listener      LS[MAXL];
static int           NLS = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv_any = PTHREAD_COND_INITIALIZER;   // señal: llegó nueva conexión

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : ".";
}

static void timestamp(char *dst, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(dst, cap, "%Y%m%d_%H%M%S", &tm);
}

static void enqueue_conn_locked(Listener *L, int cfd, struct sockaddr_in *peer) {
    ConnNode *n = (ConnNode*)calloc(1, sizeof(ConnNode));
    n->fd   = cfd;
    n->peer = *peer;
    if (!L->q_tail) {
        L->q_head = L->q_tail = n;
    } else {
        L->q_tail->next = n;
        L->q_tail = n;
    }
}

static ConnNode* dequeue_conn_locked(Listener *L) {
    ConnNode *n = L->q_head;
    if (!n) return NULL;
    L->q_head = n->next;
    if (!L->q_head) L->q_tail = NULL;
    return n;
}

static void *accept_thread(void *arg) {
    Listener *L = (Listener*)arg;

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(L->fd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("[ACCEPT] accept");
            continue;
        }

        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, peer_ip, sizeof(peer_ip));

        pthread_mutex_lock(&mtx);
        enqueue_conn_locked(L, cfd, &cli);
        pthread_cond_broadcast(&cv_any);   // avisar: hay trabajo pendiente
        pthread_mutex_unlock(&mtx);

        printf("[SERVER] encolada conexión para %s (peer %s)\n", L->name, peer_ip);
        fflush(stdout);
    }
    return NULL;
}

static void receive_file_and_save(int cfd, const char *alias) {
    // Construir ruta destino: $HOME/<alias>/recv_YYYYmmdd_HHMMSS.txt
    char ts[32];
    timestamp(ts, sizeof(ts));

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s/recv_%s.txt", home_dir(), alias, ts);

    FILE *out = fopen(path, "wb");
    if (!out) {
        perror("[RECV] fopen");
        return;
    }

    printf("[SERVER][%s] RECIBIENDO -> %s (otros esperan)\n", alias, path);
    fflush(stdout);

    char buf[BUF];
    size_t total = 0;

    for (;;) {
        ssize_t r = recv(cfd, buf, sizeof(buf), 0);
        if (r == 0) break;
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("[RECV] recv");
            break;
        }
        size_t w = fwrite(buf, 1, (size_t)r, out);
        if (w != (size_t)r) {
            perror("[RECV] fwrite");
            break;
        }
        total += (size_t)r;
    }

    fclose(out);
    close(cfd);

    printf("[SERVER][%s] COMPLETADO (%zu bytes)\n", alias, total);
    fflush(stdout);
}

static int bind_on_ip(const char *name, int port, Listener *Lout) {
    // resolver alias a IPv4
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG;

    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    int rc = getaddrinfo(name, NULL, &hints, &res);
    if (rc != 0 || !res) {
        fprintf(stderr, "getaddrinfo(%s): %s\n", name, gai_strerror(rc));
        return -1;
    }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)port);
    a.sin_addr   = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a.sin_addr, ipstr, sizeof(ipstr));
    freeaddrinfo(res);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }
    if (listen(s, BACKLOG) < 0) {
        perror("listen");
        close(s);
        return -1;
    }

    Listener L = (Listener){0};
    L.fd = s;
    snprintf(L.name, sizeof(L.name), "%s", name);
    snprintf(L.ip, sizeof(L.ip), "%s", ipstr);
    *Lout = L;

    printf("[SERVER] Escuchando en %s (%s):%d\n", name, ipstr, port);
    fflush(stdout);
    return 0;
}

// Round-robin: recorre alias 0..NLS-1
static void *scheduler_thread(void *arg) {
    (void)arg;
    int i = 0;

    for (;;) {
        pthread_mutex_lock(&mtx);

        // Si la cola del alias i está vacía, saltar al siguiente sin bloquear
        ConnNode *n = dequeue_conn_locked(&LS[i]);
        if (!n) {
            pthread_mutex_unlock(&mtx);
            i = (i + 1) % NLS;
            continue;
        }

        // Hay petición para este alias
        int cfd = n->fd;
        char alias[16];
        strncpy(alias, LS[i].name, sizeof(alias)-1);
        free(n);

        // Exclusión: liberamos el lock para recibir (los aceptadores siguen encolando)
        pthread_mutex_unlock(&mtx);

        // “Aviso” a los demás (mensaje informativo)
        printf("[SERVER] %s EN RECEPCIÓN — los demás esperan\n", alias);
        fflush(stdout);

        // Recibir el archivo COMPLETO (aunque el tiempo de turno se agote)
        receive_file_and_save(cfd, alias);

        // Siguiente alias
        i = (i + 1) % NLS;
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <PUERTO> <alias1> [alias2 alias3 ...]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 49200 s01 s02 s03 s04\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0) { fprintf(stderr, "PUERTO inválido\n"); return 1; }

    // crear listeners (uno por alias)
    for (int a = 2; a < argc && NLS < MAXL; ++a) {
        if (bind_on_ip(argv[a], port, &LS[NLS]) == 0) {
            NLS++;
        }
    }
    if (NLS == 0) {
        fprintf(stderr, "No se pudo abrir ningún listener.\n");
        return 1;
    }

    // lanzar un hilo aceptador por alias
    for (int k = 0; k < NLS; ++k) {
        pthread_t th;
        if (pthread_create(&th, NULL, accept_thread, &LS[k]) != 0) {
            perror("pthread_create accept");
            return 1;
        }
        pthread_detach(th);
    }

    // lanzar el planificador round-robin
    pthread_t sched;
    if (pthread_create(&sched, NULL, scheduler_thread, NULL) != 0) {
        perror("pthread_create scheduler");
        return 1;
    }

    // esperar al planificador (no debería terminar)
    pthread_join(sched, NULL);
    return 0;
}
