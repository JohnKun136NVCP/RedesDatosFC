// server.c — Práctica 5: servidor con turno vía monitor
// Compilar: gcc -O2 -Wall server.c -o server -lpthread
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define LINE_MAX 1024

// --- estado global ---
static volatile int busy = 0;            // 1 si estamos recibiendo archivo
static volatile int has_waiting = 0;     // 1 si hay trabajos en cola
static char g_bind_ip[64];
static int  g_control_port = 0;
static char g_dest_dir[512];
static char g_server_id[32];
static char g_mon_host[128] = "127.0.0.1";
static int  g_mon_port = 49300;


static void die(const char* m) { perror(m); exit(1); }
static void xperror(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, ": %s\n", strerror(errno));
}

static int bind_listen_ipv4(const char* ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) die("socket");
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) die("inet_pton bind_ip");
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) die("bind");
    if (listen(s, 64) < 0) die("listen");
    return s;
}

static int connect_host_port(const char* host, int port) {
    struct addrinfo hints, * res = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char p[16]; snprintf(p, sizeof(p), "%d", port);
    int e = getaddrinfo(host, p, &hints, &res);
    if (e) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e)); exit(1); }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) die("socket mon");
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) die("connect mon");
    freeaddrinfo(res);
    return s;
}

static ssize_t read_line(int s, char* buf, size_t n) {
    size_t i = 0; char c;
    while (i + 1 < n) {
        ssize_t r = recv(s, &c, 1, 0);
        if (r <= 0) return r;
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0'; return (ssize_t)i;
}
static int send_line(int s, const char* line) {
    size_t n = strlen(line);
    if (send(s, line, n, 0) < 0) return -1;
    if (n == 0 || line[n - 1] != '\n') {
        if (send(s, "\n", 1, 0) < 0) return -1;
    }
    return 0;
}

static char* basename_str(char* path) {
    char* b = strrchr(path, '/');
    return b ? b + 1 : path;
}


typedef struct Job {
    int   ctrl_sock;    // socket de control del cliente (NO cerrado hasta enviar PORT)
    char  name[512];    // nombre base
    long long size; // tamaño esperado
    struct Job* next;
} Job;

static Job* q_head = 0, * q_tail = 0;
static pthread_mutex_t q_mx = PTHREAD_MUTEX_INITIALIZER;

static void queue_push(Job* j) {
    pthread_mutex_lock(&q_mx);
    if (q_tail) q_tail->next = j; else q_head = j;
    q_tail = j; j->next = NULL;
    has_waiting = 1;
    pthread_mutex_unlock(&q_mx);
}
static Job* queue_pop() {
    pthread_mutex_lock(&q_mx);
    Job* j = q_head;
    if (j) {
        q_head = j->next;
        if (!q_head) q_tail = NULL;
    }
    has_waiting = (q_head != NULL);
    pthread_mutex_unlock(&q_mx);
    return j;
}


static off_t recv_all_to_file(int s, const char* path, long long expected) {
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { xperror("open %s", path); return -1; }
    off_t recvd = 0; char buf[1 << 15];
    while (expected < 0 || recvd < expected) {
        ssize_t want = sizeof(buf);
        if (expected >= 0) {
            long long left = expected - recvd;
            if (left < want) want = (ssize_t)left;
        }
        ssize_t r = recv(s, buf, want, 0);
        if (r < 0) { xperror("recv data"); break; }
        if (r == 0) break;
        ssize_t w = write(fd, buf, r);
        if (w != r) { xperror("write file"); break; }
        recvd += r;
    }
    close(fd);
    return recvd;
}

static int open_data_listener_ephemeral(const char* ip, int* out_port) {
    int dls = bind_listen_ipv4(ip, 0);
    struct sockaddr_in sa; socklen_t sl = sizeof(sa);
    if (getsockname(dls, (struct sockaddr*)&sa, &sl) < 0) die("getsockname");
    *out_port = ntohs(sa.sin_port);
    return dls;
}


static void* monitor_thread(void* arg) {
    (void)arg;
    int ms = connect_host_port(g_mon_host, g_mon_port);
    char hello[64]; snprintf(hello, sizeof(hello), "HELLO %s", g_server_id);
    send_line(ms, hello);

    char line[LINE_MAX];
    for (;;) {
        ssize_t n = read_line(ms, line, sizeof(line));
        if (n <= 0) { fprintf(stderr, "[server %s] monitor desconectado\n", g_server_id); exit(2); }

       
        if (strncmp(line, "TURN ", 5) == 0) {
            const char* who = line + 5;
            if (strcmp(who, g_server_id) != 0) {
                // no debería llegar, ignorar
                continue;
            }
            
            if (!has_waiting) {
                send_line(ms, "SKIP");
                continue;
            }
            
            Job* j = queue_pop();
            if (!j) {
                send_line(ms, "SKIP");
                continue;
            }
            
            send_line(ms, "START");
            busy = 1;

            
            int dataport = 0;
            int dls = open_data_listener_ephemeral(g_bind_ip, &dataport);

            char portline[64]; snprintf(portline, sizeof(portline), "PORT %d\n", dataport);
            if (send(j->ctrl_sock, portline, strlen(portline), 0) < 0) {
                xperror("send PORT");
                close(j->ctrl_sock); close(dls);
                free(j); busy = 0; send_line(ms, "DONE"); // libera de todos modos
                continue;
            }
            close(j->ctrl_sock);

            
            struct sockaddr_in c; socklen_t cl = sizeof(c);
            int ds = accept(dls, (struct sockaddr*)&c, &cl);
            close(dls);
            if (ds < 0) {
                xperror("accept datos");
                free(j); busy = 0; send_line(ms, "DONE");
                continue;
            }

            
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", g_dest_dir, j->name);

            off_t got = recv_all_to_file(ds, path, j->size);
            close(ds);

            printf("[server %s] recibido %s (%lld bytes solicitados, %lld bytes recibidos)\n",
                g_server_id, j->name, (long long)j->size, (long long)got);

            free(j);
            busy = 0;
            
            send_line(ms, "DONE");
           
        }
        else if (strncmp(line, "BUSY ", 5) == 0) {
            
            continue;
        }
        else if (strcmp(line, "IDLE") == 0) {
            
            continue;
        }
        else if (strncmp(line, "JOIN ", 5) == 0) {
            
            continue;
        }
        else {
            
        }
    }
    return NULL;
}


int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <bind_ip> <control_port> <dest_dir> <server_id> [monitor_host] [monitor_port]\n", argv[0]);
        return 1;
    }
    strncpy(g_bind_ip, argv[1], sizeof(g_bind_ip) - 1);
    g_control_port = atoi(argv[2]);
    strncpy(g_dest_dir, argv[3], sizeof(g_dest_dir) - 1);
    strncpy(g_server_id, argv[4], sizeof(g_server_id) - 1);
    if (argc >= 6) strncpy(g_mon_host, argv[5], sizeof(g_mon_host) - 1);
    if (argc >= 7) g_mon_port = atoi(argv[6]);

    if (access(g_dest_dir, W_OK) != 0) {
        fprintf(stderr, "Directorio no accesible: %s\n", g_dest_dir);
        return 1;
    }

    // hilo hacia monitor
    pthread_t th;
    if (pthread_create(&th, NULL, monitor_thread, NULL) != 0) die("pthread_create");

    int ls = bind_listen_ipv4(g_bind_ip, g_control_port);
    printf("[server %s @ %s:%d] listo. Destino: %s  Monitor: %s:%d\n",
        g_server_id, g_bind_ip, g_control_port, g_dest_dir, g_mon_host, g_mon_port);

    for (;;) {
        struct sockaddr_in c; socklen_t cl = sizeof(c);
        int cs = accept(ls, (struct sockaddr*)&c, &cl);
        if (cs < 0) { if (errno == EINTR) continue; xperror("accept ctrl"); continue; }

        char line[LINE_MAX] = { 0 };
        ssize_t n = read_line(cs, line, sizeof(line));
        if (n <= 0) { close(cs); continue; }

        if (strncmp(line, "STATUS", 6) == 0) {
            const char* resp = busy ? "RECEIVING\n" : "WAITING\n";
            send(cs, resp, strlen(resp), 0);
            close(cs);
            continue;
        }

        if (strncmp(line, "FILE ", 5) == 0) {
            // Espera: "FILE <nombre> <size>"
            char name[512] = { 0 };
            long long size = -1;
            if (sscanf(line + 5, "%511s %lld", name, &size) != 2) {
                close(cs); continue;
            }
            
            char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", name);
            char* base = basename_str(tmp);

            
            Job* j = (Job*)calloc(1, sizeof(Job));
            j->ctrl_sock = cs;
            snprintf(j->name, sizeof(j->name), "%s", base);
            j->size = size;
            queue_push(j);

            
            continue;
        }

        
        close(cs);
    }

    return 0;
}
