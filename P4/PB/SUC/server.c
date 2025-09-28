// PB server: escucha en 49200 (INADDR_ANY) para todos los alias s01..s04
// Asigna un puerto efímero por consulta (igual a Parte A) y responde STATUS.
//
// Protocolo de control (en 49200, líneas terminadas en \n):
//   - "ASSIGN": el servidor crea un socket de datos en puerto efímero y contesta
//       "PORT <numero>\n"; el cliente deberá conectarse a ese puerto y enviar "STATUS\n".
//   - "SET WAITING" | "SET RECEIVING" | "SET SENDING": actualiza el estado interno
//       (útil para pruebas). Responde "OK\n".
//   - Otros comandos: responde "ERR\n".
//
// Protocolo de datos (en el puerto efímero):
//   - Cliente envía "STATUS\n" y el servidor responde una sola línea:
//       "WAITING\n" | "RECEIVING\n" | "SENDING\n".
//
// Nota: La escucha en múltiples alias/IP se logra a nivel del sistema con los alias
// configurados; el servidor usa INADDR_ANY para atender en todas las direcciones.

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

// Crea un socket de escucha TCP en el puerto dado (IPv4, INADDR_ANY)
static int create_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

// Lee una línea del socket (hasta '\n' o EOF), escribe en buffer con terminador '\0'
static int recv_line(int fd, char *buffer, size_t maxlen) {
    size_t total = 0;
    while (total + 1 < maxlen) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        buffer[total++] = c;
        if (c == '\n') break;
    }
    buffer[total] = '\0';
    return (int)total;
}

// Crea un socket de escucha en puerto efímero (bind a puerto 0) y lo devuelve
static int assign_port(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

// Acepta una conexión en el puerto efímero y responde una única consulta STATUS
static void serve_status_once(int data_fd) {
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    int d = accept(data_fd, (struct sockaddr *)&cli, &clilen);
    if (d < 0) {
        perror("accept(data)");
        close(data_fd);
        return;
    }
    char line[BUF];
    if (recv_line(d, line, sizeof(line)) > 0) {
        if (strncmp(line, "STATUS", 6) == 0) {
            const char *resp = "WAITING\n";
            if (g_state == STATE_RECEIVING) resp = "RECEIVING\n";
            else if (g_state == STATE_SENDING) resp = "SENDING\n";
            send(d, resp, strlen(resp), 0);
        }
    }
    close(d);
    close(data_fd);
}

// Bucle principal del servidor de control (49200)
int main(void) {
    int ctrl = create_listen(CONTROL_PORT);
    if (ctrl < 0) return 1;
    printf("[PB server] listening 49200 on all aliases\n");
    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int s = accept(ctrl, (struct sockaddr *)&cli, &clilen);
        if (s < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        char line[BUF];
        int n = recv_line(s, line, sizeof(line));
        if (n <= 0) { close(s); continue; }
        // Solicitud de asignación de puerto de datos
        if (strncmp(line, "ASSIGN", 6) == 0) {
            int dfd = assign_port();
            if (dfd < 0) { close(s); continue; }
            struct sockaddr_in la; socklen_t ll = sizeof(la);
            getsockname(dfd, (struct sockaddr *)&la, &ll);
            int port = ntohs(la.sin_port);
            char resp[64];
            int m = snprintf(resp, sizeof(resp), "PORT %d\n", port);
            send(s, resp, (size_t)m, 0);
            close(s);
            serve_status_once(dfd);
        // Cambios de estado para pruebas
        } else if (strncmp(line, "SET WAITING", 11) == 0) {
            g_state = STATE_WAITING;
            const char *ok = "OK\n";
            send(s, ok, strlen(ok), 0);
            close(s);
        } else if (strncmp(line, "SET RECEIVING", 13) == 0) {
            g_state = STATE_RECEIVING;
            const char *ok = "OK\n";
            send(s, ok, strlen(ok), 0);
            close(s);
        } else if (strncmp(line, "SET SENDING", 11) == 0) {
            g_state = STATE_SENDING;
            const char *ok = "OK\n";
            send(s, ok, strlen(ok), 0);
            close(s);
        } else {
            const char *bad = "ERR\n";
            send(s, bad, strlen(bad), 0);
            close(s);
        }
    }
    close(ctrl);
    return 0;
}


