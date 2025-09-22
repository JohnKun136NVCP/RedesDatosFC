/*
  Joshua Abel Huetado Aponte
  server.c — Práctica 4 (Parte A)
*/

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#define BACKLOG  16
#define LINE_MAX 256

// Cifrado César básico (letras A-Z / a-z)
static void encryptCaesar(char *s, int shift) {
    shift %= 26;
    if (shift < 0) shift += 26;
    for (int i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isupper(c))      s[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        else if (islower(c)) s[i] = (char)(((c - 'a' + shift) % 26) + 'a');
    }
}

// Lee una línea (terminada en '\n') del socket
static ssize_t read_line(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;          
        if (r < 0)  return -1;      
        buf[n++] = c;
        if (c == '\n') break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

// Lee exactamente n bytes del socket
static int read_n(int fd, char *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r == 0) return -1;                 
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return 0;
}

// Envía todo el buffer 
static int send_all(int fd, const char *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(fd, buf + sent, n - sent, 0);
        if (w <= 0) { if (errno == EINTR) continue; return -1; }
        sent += (size_t)w;
    }
    return 0;
}

static int mklistener(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_port        = htons((uint16_t)port);
    a.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind"); close(s); return -1;
    }
    if (listen(s, BACKLOG) < 0) {
        perror("listen"); close(s); return -1;
    }
    return s;
}


static void handle_data(int cfd, int listen_port) {
    char line[LINE_MAX];

    // Esperamos la cabecera 
    if (read_line(cfd, line, sizeof(line)) <= 0) return;

    int porto = -1, shift = 0;
    size_t len = 0;
    if (sscanf(line, "PORTO=%d;SHIFT=%d;LEN=%zu", &porto, &shift, &len) != 3) {
        // Cabecera rechazada
        (void)send_all(cfd, "RECHAZADO\n", 10);
        return;
    }

    char *body = (char*)malloc(len + 1);
    if (!body) return;

    if (read_n(cfd, body, len) != 0) {
        free(body);
        return;
    }
    body[len] = '\0';

    // procesamos si porto coincide con el puerto de datos
    if (porto == listen_port) {
        encryptCaesar(body, shift);
        printf("[SERVER %d] Procesé %zu bytes (César shift=%d)\n", listen_port, len, shift);

        if (send_all(cfd, "PROCESADO\n", 11) == 0) {
            (void)send_all(cfd, body, strlen(body));
            (void)send_all(cfd, "\n", 1);
        }
    } else {
        (void)send_all(cfd, "RECHAZADO\n", 10);
    }

    free(body);
}

// main

int main(int argc, char **argv) {
    int ctrl_port = 0;
    int data_port = 0;

    if (argc == 2) {
        data_port = atoi(argv[1]);
        int ls = mklistener(data_port);
        if (ls < 0) return 1;

        printf("[+] Data listening on %d...\n", data_port);

        for (;;) {
            struct sockaddr_in cli; socklen_t cl = sizeof(cli);
            int cfd = accept(ls, (struct sockaddr*)&cli, &cl);
            if (cfd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }
            handle_data(cfd, data_port);
            close(cfd);
        }
        close(ls);
        return 0;

    } else if (argc == 3) {
        ctrl_port = atoi(argv[1]);  
        data_port = atoi(argv[2]);  

        int ls_ctrl = mklistener(ctrl_port);
        int ls_data = mklistener(data_port);
        if (ls_ctrl < 0 || ls_data < 0) return 1;

        printf("[+] Control listening on %d...\n", ctrl_port);
        printf("[+] Data    listening on %d...\n", data_port);

        for (;;) {
            fd_set r; FD_ZERO(&r);
            FD_SET(ls_ctrl, &r);
            FD_SET(ls_data, &r);
            int maxfd = (ls_ctrl > ls_data ? ls_ctrl : ls_data);

            if (select(maxfd + 1, &r, NULL, NULL, NULL) < 0) {
                if (errno == EINTR) continue;
                perror("select");
                break;
            }
            if (FD_ISSET(ls_ctrl, &r)) {
                struct sockaddr_in cli; socklen_t cl = sizeof(cli);
                int cfd = accept(ls_ctrl, (struct sockaddr*)&cli, &cl);
                if (cfd >= 0) {
                    char out[64];
                    int n = snprintf(out, sizeof(out), "NEWPORT=%d\n", data_port);
                    (void)send_all(cfd, out, (size_t)n);
                    close(cfd);
                }
            }
            if (FD_ISSET(ls_data, &r)) {
                struct sockaddr_in cli; socklen_t cl = sizeof(cli);
                int cfd = accept(ls_data, (struct sockaddr*)&cli, &cl);
                if (cfd >= 0) {
                    handle_data(cfd, data_port);
                    close(cfd);
                }
            }
        }

        close(ls_ctrl);
        close(ls_data);
        return 0;
    }

    fprintf(stderr,
            "Uso:\n"
            "  %s <PUERTO_DATOS>\n"
            "  %s 49200 <PUERTO_DATOS>\n",
            argv[0], argv[0]);
    return 1;
}
//finnn
