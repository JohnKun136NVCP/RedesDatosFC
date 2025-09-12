#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSZ 4096

static int recv_all(int fd, void* buf, size_t len) {
    char* p = (char*)buf; size_t r = 0;
    while (r < len) {
        ssize_t n = recv(fd, p + r, len - r, 0);
        if (n == 0) return 0; if (n < 0) { if (errno == EINTR) continue; return -1; } r += n;
    }
    return 1;
}
static int send_all(int fd, const void* buf, size_t len) {
    const char* p = (const char*)buf; size_t s = 0;
    while (s < len) {
        ssize_t n = send(fd, p + s, len - s, 0);
        if (n <= 0) { if (errno == EINTR) continue; return -1; } s += n;
    }
    return 1;
}
static char* load_file(const char* path, int* len) {
    struct stat st; if (stat(path, &st) < 0) return NULL;
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    char* buf = (char*)malloc((size_t)st.st_size + 1); if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)st.st_size, f); fclose(f); buf[n] = '\0'; *len = (int)n; return buf;
}
static int talk(const char* ip, int server_port, int target_port, int shift,
    const char* data, int len) {
    int s = socket(AF_INET, SOCK_STREAM, 0); if (s < 0) { perror("socket"); return -1; }
    struct sockaddr_in sa = { 0 }; sa.sin_family = AF_INET; sa.sin_port = htons((uint16_t)server_port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) <= 0) { perror("inet_pton"); close(s); return -1; }
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) < 0) { perror("connect"); close(s); return -1; }
    printf("[+] Connected %s:%d\n", ip, server_port);

    uint32_t net_target = htonl((uint32_t)target_port);
    uint32_t net_shift = htonl((uint32_t)shift);
    uint32_t net_len = htonl((uint32_t)len);
    if (!send_all(s, &net_target, 4) || !send_all(s, &net_shift, 4) ||
        !send_all(s, &net_len, 4) || !send_all(s, data, (size_t)len)) {
        perror("send_all"); close(s); return -1;
    }

    uint32_t net_status; if (!recv_all(s, &net_status, 4)) { close(s); return -1; }
    int status = (int)ntohl(net_status);
    if (status == 1) {
        uint32_t net_plen; if (!recv_all(s, &net_plen, 4)) { close(s); return -1; }
        int plen = (int)ntohl(net_plen);
        char* buf = (char*)malloc((size_t)plen + 1); if (!buf) { close(s); return -1; }
        if (!recv_all(s, buf, (size_t)plen)) { free(buf); close(s); return -1; }
        buf[plen] = '\0';
        printf("[Client] Server %d -> PROCESADO (primeros 120):\n%.120s\n", server_port, buf);
        // guarda archivo
        char out[256]; snprintf(out, sizeof(out), "info_%d.txt", server_port);
        FILE* f = fopen(out, "wb"); if (f) {
            fwrite(buf, 1, (size_t)plen, f); fclose(f);
            printf("[Client] Guardado en %s\n", out);
        }
        free(buf);
    }
    else {
        printf("[Client] Server %d -> RECHAZADO\n", server_port);
    }
    close(s); return 0;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <archivo.txt>\n", argv[0]);
        return 1;
    }
    const char* ip = argv[1]; int target_port = atoi(argv[2]); int shift = atoi(argv[3]);
    int len = 0; char* data = load_file(argv[4], &len); if (!data) { perror("archivo"); return 1; }

    int ports[3] = { 49200,49201,49202 };
    for (int i = 0; i < 3; i++) talk(ip, ports[i], target_port, shift, data, len);
    free(data); return 0;
}
