#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

// Servidor sencillo de recepción de archivos con turnos round-robin
// Solo se procesa UNA transferencia a la vez (exclusión mutua).
// Cada conexión entrante envía en una sola línea los metadatos destino:
//   "INDICE FILENAME SIZE\n"
// Un planificador (scheduler) recorre los índices de servidores lógicos; cuando
// un servidor tiene una solicitud en cola y es su turno, se le otorga el turno,
// se responde "READY\n" al cliente y se recibe el archivo completo.

/*
Instrucciones de compilación y uso

Compilación (macOS con clang):
    clang -Wall -Wextra -O2 P5/SUC/server.c -o server -lpthread

Compilación (Linux con gcc):
    gcc -Wall -Wextra -O2 P5/SUC/server.c -o server -lpthread

Ejecución básica:
    ./server -p 9200 -n 3

Protocolo de aplicación:
    Cabecera del cliente (una línea): "INDICE FILENAME SIZE\n"
    Respuesta del servidor al conceder turno: "READY\n"
    Luego el cliente envía exactamente SIZE bytes del archivo.

Demostración round-robin (múltiples clientes):
    ./client 127.0.0.1 9200 0 a.txt &
    ./client 127.0.0.1 9200 1 b.txt &
    ./client 127.0.0.1 9200 2 c.txt &
*/

typedef long long ll;

typedef struct ConnectionRequest {
    int client_fd;
    int server_index;
    ll file_size;
    char filename[256];
    struct sockaddr_in client_addr;
    struct ConnectionRequest *next;
} ConnectionRequest;

typedef struct {
    ConnectionRequest *head;
    ConnectionRequest *tail;
    pthread_mutex_t mutex;
} RequestQueue;

static int g_listen_fd = -1;
static int g_num_servers = 3;   // número de servidores lógicos
static int g_port = 9090;       // puerto de escucha
static volatile int g_running = 1;
static RequestQueue *g_queues = NULL; // una cola por servidor lógico

static void queue_init(RequestQueue *q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
}

static void queue_push(RequestQueue *q, ConnectionRequest *req) {
    req->next = NULL;
    pthread_mutex_lock(&q->mutex);
    if (q->tail) {
        q->tail->next = req;
    } else {
        q->head = req;
    }
    q->tail = req;
    pthread_mutex_unlock(&q->mutex);
}

static ConnectionRequest* queue_pop(RequestQueue *q) {
    pthread_mutex_lock(&q->mutex);
    ConnectionRequest *n = q->head;
    if (n) {
        q->head = n->next;
        if (!q->head) q->tail = NULL;
    }
    pthread_mutex_unlock(&q->mutex);
    return n;
}

static int mkdir_p(const char *path) {
    // Crea directorios anidados (uploads/server_X) si no existen
    char buf[512];
    size_t len = strnlen(path, sizeof(buf));
    if (len == 0 || len >= sizeof(buf)) return -1;
    strncpy(buf, path, sizeof(buf));
    for (size_t i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0755) < 0 && errno != EEXIST) return -1;
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0755) < 0 && errno != EEXIST) return -1;
    return 0;
}

static ssize_t read_line(int fd, char *out, size_t cap) {
    // Lee del socket hasta nueva línea (\n) o hasta agotar capacidad
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) break; // EOF del socket
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        out[used++] = c;
        if (c == '\n') break;
    }
    out[used] = '\0';
    return (ssize_t)used;
}

static int parse_header(const char *line, int *server_idx, char *filename, size_t fn_cap, ll *file_size) {
    // Formato esperado: INDICE FILENAME SIZE\n
    // El nombre de archivo puede contener espacios; estrategia:
    // - Primer token: índice
    // - Último token: tamaño
    // - Tokens intermedios: componen el nombre de archivo
    char tmp[1024];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp)-1] = '\0';

    // Quitar CR/LF finales
    size_t L = strlen(tmp);
    while (L > 0 && (tmp[L-1] == '\n' || tmp[L-1] == '\r')) tmp[--L] = '\0';

    // Tokenizar por espacios
    char *tokens[128];
    int tcount = 0;
    char *saveptr = NULL;
    char *p = strtok_r(tmp, " ", &saveptr);
    while (p && tcount < 128) {
        tokens[tcount++] = p;
        p = strtok_r(NULL, " ", &saveptr);
    }
    if (tcount < 3) return -1;

    char *endp = NULL;
    long idx = strtol(tokens[0], &endp, 10);
    if (*endp != '\0' || idx < 0) return -1;
    *server_idx = (int)idx;

    // Tamaño es el último token
    endp = NULL;
    long long sz = strtoll(tokens[tcount-1], &endp, 10);
    if (*endp != '\0' || sz < 0) return -1;
    *file_size = (ll)sz;

    // Nombre de archivo = tokens[1..tcount-2] unidos por espacios
    filename[0] = '\0';
    size_t pos = 0;
    for (int i = 1; i <= tcount - 2; ++i) {
        size_t len = strlen(tokens[i]);
        if (pos + len + 1 >= fn_cap) return -1;
        memcpy(filename + pos, tokens[i], len);
        pos += len;
        if (i != tcount - 2) filename[pos++] = ' ';
    }
    filename[pos] = '\0';
    return 0;
}

static void *accept_thread_fn(void *arg) {
    (void)arg;
    while (g_running) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int cfd = accept(g_listen_fd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        // Leer línea de cabecera
        char line[1024];
        ssize_t ln = read_line(cfd, line, sizeof(line));
        if (ln <= 0) {
            close(cfd);
            continue;
        }
        int idx = -1; ll fsz = 0; char fname[256];
        if (parse_header(line, &idx, fname, sizeof(fname), &fsz) != 0) {
            const char *err = "ERR header\n";
            send(cfd, err, strlen(err), 0);
            close(cfd);
            continue;
        }
        if (idx < 0 || idx >= g_num_servers) {
            const char *err = "ERR index\n";
            send(cfd, err, strlen(err), 0);
            close(cfd);
            continue;
        }

        ConnectionRequest *req = (ConnectionRequest *)calloc(1, sizeof(ConnectionRequest));
        if (!req) { close(cfd); continue; }
        req->client_fd = cfd;
        req->server_index = idx;
        req->file_size = fsz;
        strncpy(req->filename, fname, sizeof(req->filename)-1);
        req->client_addr = cli;
        queue_push(&g_queues[idx], req);
        // No respondemos aquí; el scheduler enviará READY cuando sea el turno
    }
    return NULL;
}

static int receive_exact(int fd, void *buf, size_t len) {
    // Utilidad para leer exactamente len bytes del socket (no usada actualmente)
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, (char*)buf + got, len - got, 0);
        if (n == 0) return -1; // EOF prematuro
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)n;
    }
    return 0;
}

static int receive_file(ConnectionRequest *req, int server_idx) {
    // Recibe el archivo completo y lo guarda en uploads/server_<idx>/
    char dir[256];
    snprintf(dir, sizeof(dir), "uploads/server_%d", server_idx);
    if (mkdir_p(dir) != 0) {
        perror("mkdir_p");
        return -1;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, req->filename);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Avisar al cliente que puede empezar a enviar los bytes
    char ready[] = "READY\n";
    if (send(req->client_fd, ready, sizeof(ready)-1, 0) < 0) {
        perror("send READY");
        fclose(fp);
        return -1;
    }

    const size_t BUF = 64 * 1024;
    char *buffer = (char*)malloc(BUF);
    if (!buffer) { fclose(fp); return -1; }

    ll remaining = req->file_size;
    while (remaining > 0) {
        size_t chunk = (remaining > (ll)BUF) ? BUF : (size_t)remaining;
        ssize_t n = recv(req->client_fd, buffer, chunk, 0);
        if (n == 0) { free(buffer); fclose(fp); return -1; }
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv data");
            free(buffer); fclose(fp); return -1;
        }
        size_t written = fwrite(buffer, 1, (size_t)n, fp);
        if (written != (size_t)n) {
            perror("fwrite");
            free(buffer); fclose(fp); return -1;
        }
        remaining -= (ll)n;
    }
    free(buffer);
    fclose(fp);
    return 0;
}

static void log_ts(const char *msg) {
    // Log con sello de tiempo HH:MM:SS
    time_t now = time(NULL);
    struct tm t; localtime_r(&now, &t);
    char ts[32]; strftime(ts, sizeof(ts), "%H:%M:%S", &t);
    printf("[%s] %s\n", ts, msg);
    fflush(stdout);
}

static void *scheduler_thread_fn(void *arg) {
    (void)arg;
    int current = 0;
    while (g_running) {
        // Round-robin: si la cola actual está vacía, avanzar inmediatamente
        ConnectionRequest *req = queue_pop(&g_queues[current]);
        if (!req) {
            current = (current + 1) % g_num_servers;
            // Pequeño sleep para evitar consumir CPU si no hay trabajo
            struct timespec ts = {0, 50 * 1000 * 1000}; // 50ms
            nanosleep(&ts, NULL);
            continue;
        }

        char info[256];
        snprintf(info, sizeof(info), "Turno servidor %d: iniciando recepcion de '%s' (%lld bytes) de %s:%d",
                 current, req->filename, (long long)req->file_size,
                 inet_ntoa(req->client_addr.sin_addr), ntohs(req->client_addr.sin_port));
        log_ts(info);
        log_ts("Notificando a los demas: servidor en recepcion (exclusion mutua activa)");

        int rc = receive_file(req, current);
        if (rc == 0) {
            log_ts("Recepcion finalizada. Liberando turno.");
        } else {
            log_ts("Error en recepcion. Cerrando conexion y liberando turno.");
        }
        close(req->client_fd);
        free(req);

        // Siguiente servidor lógico en el ciclo
        current = (current + 1) % g_num_servers;
    }
    return NULL;
}

static int setup_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1; }
    if (listen(fd, 64) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s [-p port] [-n servers]\n", prog);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "p:n:h")) != -1) {
        switch (opt) {
            case 'p': g_port = atoi(optarg); break;
            case 'n': g_num_servers = atoi(optarg); break;
            case 'h': default: usage(argv[0]); return 1;
        }
    }
    if (g_num_servers <= 0) g_num_servers = 1;

    g_queues = (RequestQueue*)calloc((size_t)g_num_servers, sizeof(RequestQueue));
    if (!g_queues) { perror("calloc queues"); return 1; }
    for (int i = 0; i < g_num_servers; ++i) queue_init(&g_queues[i]);

    g_listen_fd = setup_listener(g_port);
    if (g_listen_fd < 0) return 1;
    printf("Servidor escuchando en puerto %d, servidores virtuales: %d\n", g_port, g_num_servers);
    fflush(stdout);

    pthread_t th_accept, th_sched;
    if (pthread_create(&th_accept, NULL, accept_thread_fn, NULL) != 0) {
        perror("pthread_create accept");
        return 1;
    }
    if (pthread_create(&th_sched, NULL, scheduler_thread_fn, NULL) != 0) {
        perror("pthread_create scheduler");
        return 1;
    }

    // Esperar indefinidamente (Ctrl+C para terminar el proceso)
    pthread_join(th_accept, NULL);
    pthread_join(th_sched, NULL);
    close(g_listen_fd);
    return 0;
}


