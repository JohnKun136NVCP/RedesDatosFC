#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

#define BACKLOG 16
#define MAX_ALIAS 32
#define BUF_SZ 4096
#define TURN_TIME_MS 5000  // 5 segundos por turno

/* Nodo para cola de conexiones */
typedef struct ConnNode {
    int cfd;
    struct ConnNode *next;
} ConnNode;

/* Estructura por alias */
typedef struct Alias {
    char name[32];
    char ip[64];
    int lfd;
    ConnNode *head, *tail;
    pthread_mutex_t qlock;
} Alias;

/* Variables globales */
static Alias aliases[MAX_ALIAS];
static int n_alias = 0;
static pthread_t th_acceptor, th_scheduler;
static atomic_int running = 1;
static atomic_int turn_index = 0;
static atomic_int current_receiver = -1;

/* Logging  */
static void log_msg(const char *alias, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%02d:%02d:%02d][%s] ", t->tm_hour, t->tm_min, t->tm_sec, 
            alias ? alias : "srv");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

/* Asegura directorio */
static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

/* Encola una nueva conexión */
static void enqueue_conn(int idx, int cfd) {
    ConnNode *n = calloc(1, sizeof(ConnNode));
    if (!n) { 
        close(cfd); 
        return; 
    }
    n->cfd = cfd;
    n->next = NULL;
    
    pthread_mutex_lock(&aliases[idx].qlock);
    if (!aliases[idx].tail) {
        aliases[idx].head = aliases[idx].tail = n;
    } else {
        aliases[idx].tail->next = n;
        aliases[idx].tail = n;
    }
    pthread_mutex_unlock(&aliases[idx].qlock);
}

/* Saca una conexión de la cola */
static int dequeue_conn(int idx) {
    pthread_mutex_lock(&aliases[idx].qlock);
    ConnNode *h = aliases[idx].head;
    if (!h) { 
        pthread_mutex_unlock(&aliases[idx].qlock); 
        return -1; 
    }
    aliases[idx].head = h->next;
    if (!aliases[idx].head) aliases[idx].tail = NULL;
    pthread_mutex_unlock(&aliases[idx].qlock);
    
    int cfd = h->cfd;
    free(h);
    return cfd;
}

static ssize_t read_n(int fd, void *buf, size_t n) {
    size_t total = 0;
    char *p = buf;
    while (total < n) {
        ssize_t r = recv(fd, p + total, n - total, 0);
        if (r == 0) return (ssize_t)total; /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)r;
    }
    return (ssize_t)total;
}

/* Maneja una conexión completa */
static void handle_connection(int idx, int cfd) {
    const char *alias = aliases[idx].name;
    
    atomic_store(&current_receiver, idx);

    /* 1) leer int filename_len */
    int filename_len;
    ssize_t r = read_n(cfd, &filename_len, sizeof(filename_len));
    if (r <= 0) {
        log_msg(alias, "Cliente cerró antes de enviar filename_len");
        atomic_store(&current_receiver, -1);
        close(cfd);
        return;
    }
    
    // Convertir de network byte order
    filename_len = ntohl(filename_len);
    
    if (filename_len <= 0 || filename_len > 1024) {
        log_msg(alias, "Filename_len inválido: %d", filename_len);
        atomic_store(&current_receiver, -1);
        close(cfd);
        return;
    }

    /* 2) leer nombre del archivo */
    char fname[1025];
    r = read_n(cfd, fname, (size_t)filename_len);
    if (r <= 0) {
        log_msg(alias, "No se leyó nombre de archivo correctamente");
        atomic_store(&current_receiver, -1);
        close(cfd);
        return;
    }
    fname[filename_len] = '\0';

    /* Crear carpeta en HOME/<alias> */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char dirpath[512];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", home, alias);
    ensure_dir(dirpath);

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, fname);

    FILE *out = fopen(filepath, "wb");
    if (!out) {
        log_msg(alias, "Error fopen %s: %s", filepath, strerror(errno));
        atomic_store(&current_receiver, -1);
        close(cfd);
        return;
    }

    log_msg(alias, "Recibiendo: %s", fname);

    /* 3) leer datos hasta EOF */
    char buf[BUF_SZ];
    ssize_t got;
    size_t total = 0;
    while ((got = recv(cfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, (size_t)got, out);
        total += (size_t)got;
    }
    fclose(out);
    close(cfd);

    if (got == 0) {
        log_msg(alias, "Completado: %s (%zu bytes)", fname, total);
    } else {
        log_msg(alias, "Incompleto: %s (%zu bytes)", fname, total);
    }
    
    atomic_store(&current_receiver, -1);
}

/* Thread aceptador */
static void *acceptor_fn(void *arg) {
    (void)arg;
    
    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int mx = -1;
        
        for (int i = 0; i < n_alias; ++i) {
            FD_SET(aliases[i].lfd, &rfds);
            if (aliases[i].lfd > mx) mx = aliases[i].lfd;
        }
        
        struct timeval tv = {1, 0};
        int rc = select(mx + 1, &rfds, NULL, NULL, &tv);
        
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (rc == 0) continue;
        
        for (int i = 0; i < n_alias; ++i) {
            if (FD_ISSET(aliases[i].lfd, &rfds)) {
                struct sockaddr_in cli;
                socklen_t cl = sizeof(cli);
                int cfd = accept(aliases[i].lfd, (struct sockaddr*)&cli, &cl);
                if (cfd < 0) {
                    if (errno == EINTR) continue;
                    continue;
                }
                
                enqueue_conn(i, cfd);
                log_msg(aliases[i].name, "Conexión recibida");
            }
        }
    }
    
    return NULL;
}

/* Scheduler: Round-Robin con tiempo  */
static void *scheduler_fn(void *arg) {
    (void)arg;
    
    while (running) {
        int idx = atomic_load(&turn_index) % n_alias;
        const char *current_alias = aliases[idx].name;
        int receiver = atomic_load(&current_receiver);
        
        // Verificar si alguien más está recibiendo
        if (receiver >= 0 && receiver != idx) {
            usleep(100000);
            continue;
        }
        
        int cfd = dequeue_conn(idx);
        if (cfd >= 0) {
            handle_connection(idx, cfd);
        } else {
            int next_turn = (atomic_load(&turn_index) + 1) % n_alias;
            log_msg(current_alias, "Sin conexiones - Siguiente: %s", aliases[next_turn].name);
        }
        
        // AVANZAR TURNO (Round-Robin)
        atomic_store(&turn_index, (atomic_load(&turn_index) + 1) % n_alias);
        
        usleep(1000 * TURN_TIME_MS);
    }
    
    return NULL;
}

/* Resuelve nombre a IPv4 */
static int resolve4(const char *name, struct in_addr *out) {
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    if (getaddrinfo(name, NULL, &hints, &res) != 0) return -1;
    if (!res) return -1;
    struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;
    *out = sin->sin_addr;
    freeaddrinfo(res);
    return 0;
}

/* Crear listener */
static int make_listener(struct in_addr ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr = ip;
    a.sin_port = htons((uint16_t)port);
    
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        close(s);
        return -1;
    }
    
    if (listen(s, BACKLOG) < 0) { 
        close(s); 
        return -1; 
    }
    
    return s;
}

/* Manejo de señales */
static void handle_signal(int sig) {
    log_msg("sys", "Terminando servidor...");
    atomic_store(&running, 0);
}

/* MAIN */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <PUERTO> [alias1] [alias2] ...\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) { 
        fprintf(stderr, "Puerto inválido: %d\n", port); 
        return 1; 
    }

    // Configurar manejo de señales
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    /* Alias específicos */
    if (argc > 2) {
        printf("Iniciando servidor en puerto %d con %d aliases\n", port, argc - 2);
        
        for (int i = 2; i < argc && n_alias < MAX_ALIAS; ++i) {
            struct in_addr ip;
            if (resolve4(argv[i], &ip) != 0) {
                fprintf(stderr, "No se pudo resolver: %s\n", argv[i]);
                continue;
            }
            
            int lfd = make_listener(ip, port);
            if (lfd < 0) {
                fprintf(stderr, "No se pudo crear listener para: %s:%d\n", argv[i], port);
                continue;
            }
            
            strncpy(aliases[n_alias].name, argv[i], sizeof(aliases[n_alias].name)-1);
            inet_ntop(AF_INET, &ip, aliases[n_alias].ip, sizeof(aliases[n_alias].ip));
            aliases[n_alias].lfd = lfd;
            aliases[n_alias].head = aliases[n_alias].tail = NULL;
            pthread_mutex_init(&aliases[n_alias].qlock, NULL);
            
            printf("Listener: %s (%s:%d)\n", aliases[n_alias].name, aliases[n_alias].ip, port);
            n_alias++;
        }
        
        if (n_alias == 0) { 
            fprintf(stderr, "No se crearon listeners válidos\n"); 
            return 1; 
        }
    }

    /* Iniciar hilos */
    if (pthread_create(&th_acceptor, NULL, acceptor_fn, NULL) != 0) {
        perror("pthread_create acceptor"); 
        return 1;
    }
    
    if (pthread_create(&th_scheduler, NULL, scheduler_fn, NULL) != 0) {
        perror("pthread_create scheduler"); 
        atomic_store(&running, 0);
        return 1;
    }

    
    /* Loop principal */
    while (atomic_load(&running)) {
        sleep(1);
    }

    pthread_join(th_acceptor, NULL);
    pthread_join(th_scheduler, NULL);
    
    for (int i = 0; i < n_alias; ++i) {
        close(aliases[i].lfd);
        pthread_mutex_destroy(&aliases[i].qlock);
        
        ConnNode *node = aliases[i].head;
        while (node) {
            ConnNode *next = node->next;
            close(node->cfd);
            free(node);
            node = next;
        }
    }
    
    return 0;
}