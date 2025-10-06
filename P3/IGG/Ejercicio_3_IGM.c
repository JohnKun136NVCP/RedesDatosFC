#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
static const int PORTS[3] = {49200, 49201, 49202};

typedef struct {
    const char *ip;
    int server_port;
    int target_port;
    int shift;
    const char *content;
} Task;

static ssize_t send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

static char* read_and_sanitize(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { perror("fopen"); return NULL; }
    struct stat st; if (stat(path, &st) != 0) { perror("stat"); fclose(f); return NULL; }
    size_t cap = (size_t)st.st_size + 1;
    char *raw = (char*)malloc(cap); if (!raw) { fclose(f); return NULL; }
    size_t n = fread(raw, 1, cap - 1, f); raw[n] = '\0'; fclose(f);
    char *clean = (char*)malloc(n + 1); if (!clean) { free(raw); return NULL; }
    size_t j = 0; for (size_t i = 0; i < n; ++i) { unsigned char c = (unsigned char)raw[i]; if (isalpha(c)) clean[j++] = (char)tolower(c); }
    clean[j] = '\0'; free(raw); return clean;
}

static void* worker(void *arg) {
    Task *t = (Task*)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return NULL; }
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)t->server_port);
    if (inet_pton(AF_INET, t->ip, &addr.sin_addr) != 1) { fprintf(stderr, "inet_pton fallo para %s\n", t->ip); close(sock); return NULL; }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { fprintf(stderr, "Conexión fallida al puerto %d\n", t->server_port); close(sock); return NULL; }
    size_t needed = strlen(t->content) + 64;
    char *msg = (char*)malloc(needed); snprintf(msg, needed, "%d %d %s", t->target_port, t->shift, t->content);
    if (send_all(sock, msg, strlen(msg)) < 0) { perror("send_all"); free(msg); close(sock); return NULL; }
    char buf[BUFFER_SIZE]; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n > 0) { buf[n] = '\0'; printf("[srv %d] %s\n", t->server_port, buf); } else { printf("[srv %d] sin respuesta\n", t->server_port); }
    free(msg); close(sock); return NULL;
}

int main(int argc, char **argv) {
    if (argc < 5 || ((argc - 2) % 3) != 0) {
        fprintf(stderr, "Uso: %s <IP> <p1> <s1> <f1> [<p2> <s2> <f2> ...]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];

    for (int i = 2; i < argc; i += 3) {
        int target_port = atoi(argv[i]);
        int shift = atoi(argv[i+1]);
        const char *path = argv[i+2];

        char *clean = read_and_sanitize(path);
        if (!clean) { fprintf(stderr, "No se pudo leer %s\n", path); continue; }

        pthread_t th[3];
        Task tasks[3];
        for (int k = 0; k < 3; ++k) {
            tasks[k].ip = ip; tasks[k].server_port = PORTS[k];
            tasks[k].target_port = target_port; tasks[k].shift = shift; tasks[k].content = clean;
            pthread_create(&th[k], NULL, worker, &tasks[k]);
        }
        for (int k = 0; k < 3; ++k) pthread_join(th[k], NULL);
        free(clean); puts("");
    }
    return 0;
}
