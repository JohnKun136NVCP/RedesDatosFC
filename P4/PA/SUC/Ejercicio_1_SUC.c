// Server_PA: escucha en puerto de control (por ejemplo 49200) en múltiples IP/alias
// Asigna un puerto efímero (>49200) para cada cliente y mantiene estado simple
// Estados: WAITING, RECEIVING <filename>, SENDING <filename>
// Protocolo control (líneas): ASSIGN -> devuelve "PORT <p>\n"
// Protocolo data (STATUS?): el cliente se conecta a <p> y consulta "STATUS\n"
// Responde: "WAITING" | "RECEIVING" | "SENDING"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONTROL_PORT 49200
#define BACKLOG 64
#define BUF 1024

typedef enum { STATE_WAITING = 0, STATE_RECEIVING, STATE_SENDING } server_state_t;

static server_state_t g_state = STATE_WAITING;
static char g_current_file[256] = {0};

static int create_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int yes = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, BACKLOG) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

static int recv_line(int fd, char *buf, size_t max) {
    size_t t = 0; while (t + 1 < max) {
        char c; ssize_t r = recv(fd, &c, 1, 0);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break; buf[t++] = c; if (c == '\n') break;
    }
    buf[t] = '\0'; return (int)t;
}

static int assign_ephemeral_port(void) {
    // Pide un socket con puerto 0 (kernel asigna) y vuelve a escuchar en ese puerto
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int yes = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(0);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, BACKLOG) < 0) { perror("listen"); close(fd); return -1; }
    struct sockaddr_in la; socklen_t ll = sizeof(la);
    if (getsockname(fd, (struct sockaddr *)&la, &ll) < 0) { perror("getsockname"); close(fd); return -1; }
    return fd; // el puerto real se obtiene con getsockname cuando haga falta
}

static void run_data_loop(int data_fd) {
    // Acepta UNA conexión y responde estado actual
    struct sockaddr_in cli; socklen_t cl = sizeof(cli);
    int cfd = accept(data_fd, (struct sockaddr *)&cli, &cl);
    if (cfd < 0) { perror("accept(data)"); close(data_fd); return; }
    char line[BUF]; int n = recv_line(cfd, line, sizeof(line));
    if (n > 0) {
        if (strncmp(line, "STATUS", 6) == 0) {
            const char *resp = "WAITING\n";
            if (g_state == STATE_RECEIVING) resp = "RECEIVING\n";
            else if (g_state == STATE_SENDING) resp = "SENDING\n";
            send(cfd, resp, strlen(resp), 0);
        }
    }
    close(cfd);
    close(data_fd);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    int ctrl_fd = create_listen(CONTROL_PORT);
    if (ctrl_fd < 0) return 1;
    printf("[SERVER_PA] Listening on control port %d (all aliases)\n", CONTROL_PORT);

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(ctrl_fd, (struct sockaddr *)&cli, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

        char line[BUF]; int n = recv_line(cfd, line, sizeof(line));
        if (n <= 0) { close(cfd); continue; }

        if (strncmp(line, "ASSIGN", 6) == 0) {
            int data_fd = assign_ephemeral_port();
            if (data_fd < 0) { close(cfd); continue; }
            struct sockaddr_in la; socklen_t ll = sizeof(la);
            getsockname(data_fd, (struct sockaddr *)&la, &ll);
            int port = ntohs(la.sin_port);
            char resp[64]; int m = snprintf(resp, sizeof(resp), "PORT %d\n", port);
            send(cfd, resp, (size_t)m, 0);
            close(cfd);
            // Atender una sola consulta STATUS en ese puerto
            run_data_loop(data_fd);
        } else if (strncmp(line, "SET RECEIVING", 13) == 0) {
            g_state = STATE_RECEIVING; strncpy(g_current_file, "file", sizeof(g_current_file)-1);
            const char *ok = "OK\n"; send(cfd, ok, strlen(ok), 0); close(cfd);
        } else if (strncmp(line, "SET SENDING", 11) == 0) {
            g_state = STATE_SENDING; strncpy(g_current_file, "file", sizeof(g_current_file)-1);
            const char *ok = "OK\n"; send(cfd, ok, strlen(ok), 0); close(cfd);
        } else if (strncmp(line, "SET WAITING", 11) == 0) {
            g_state = STATE_WAITING; g_current_file[0] = '\0';
            const char *ok = "OK\n"; send(cfd, ok, strlen(ok), 0); close(cfd);
        } else {
            const char *bad = "ERR\n"; send(cfd, bad, strlen(bad), 0); close(cfd);
        }
    }

    close(ctrl_fd);
    return 0;
}


