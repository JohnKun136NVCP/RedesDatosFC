// serverOpt.c – Práctica 3
// ./serverOpt

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Cifrado César 
static void cesar(char *s, int k) {
    k %= 26; if (k < 0) k += 26;
    for (int i = 0; s[i]; i++) {
        unsigned char c = s[i];
        if (isupper(c))      s[i] = ((c - 'A' + k) % 26) + 'A';
        else if (islower(c)) s[i] = ((c - 'a' + k) % 26) + 'a';
    }
}

// lectura y escritura

static ssize_t read_line(int fd, char *buf, size_t cap) {
    size_t n = 0; char c;
    while (n + 1 < cap) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;      
        if (r < 0)  return -1;  // error
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
        if (r == 0) return -1;  // se cerró antes de tiempo
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

// socket de escucha 
static int mklistener(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)port);
    a.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind"); close(s); return -1;
    }
    if (listen(s, 16) < 0) {
        perror("listen"); close(s); return -1;
    }
    printf("[*] Escuchando en %d\n", port);
    return s;
}


// Atender una conexión
static void atiende(int cfd, int miPuerto) {
    char linea[256];
    if (read_line(cfd, linea, sizeof(linea)) <= 0) return;

    int porto = -1, shift = 0;
    size_t len = 0;
    if (sscanf(linea, "PORTO=%d;SHIFT=%d;LEN=%zu", &porto, &shift, &len) != 3) {
        // Si la cabecera no tiene el formato esperado, se rechaza
        send_all(cfd, "RECHAZADO\n", 10);
        return;
    }

    // lee exactamente LEN bytes del cuerpo
    char *msg = (char*)malloc(len + 1);
    if (!msg) return;
    if (read_n(cfd, msg, len) != 0) { free(msg); return; }
    msg[len] = '\0';

    // Si PORTO coincide con el puerto que se acepta en la conexión, se procesa
    if (porto == miPuerto) {
        cesar(msg, shift);
        printf("[SERVER %d] Encrypted:\n%s\n", miPuerto, msg);

        send_all(cfd, "PROCESADO\n", 11);
        send_all(cfd, msg, strlen(msg));
        send_all(cfd, "\n", 1); 
    } else {
        send_all(cfd, "RECHAZADO\n", 10);
    }

    free(msg);
}

int main(void) {
    // tres puertos 
    int puertos[3] = {49200, 49201, 49202};
    int ls[3];

    // listener por puerto
    for (int i = 0; i < 3; i++) {
        ls[i] = mklistener(puertos[i]);
        if (ls[i] < 0) return 1;
    }
    puts("[*] ServerOpt listo (select). Ctrl+C para salir.");

    // se espera algo en cualquiera de los 3 sockets
    for (;;) {
        fd_set r; FD_ZERO(&r);
        int maxfd = -1;
        for (int i = 0; i < 3; i++) {
            FD_SET(ls[i], &r);
            if (ls[i] > maxfd) maxfd = ls[i];
        }

        if (select(maxfd + 1, &r, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i < 3; i++) {
            if (!FD_ISSET(ls[i], &r)) continue;

            struct sockaddr_in cli; socklen_t cl = sizeof(cli);
            int cfd = accept(ls[i], (struct sockaddr*)&cli, &cl);
            if (cfd < 0) { perror("accept"); continue; }

            atiende(cfd, puertos[i]);
            close(cfd);
        }
    }

    for (int i = 0; i < 3; i++) close(ls[i]);
    return 0;
}

/finnn
