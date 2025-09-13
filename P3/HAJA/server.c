// server.c – Práctica 3 Redes
// ./server <PUERTO>
// Si PORTO coincide con mi puerto, se cifra con César y se responde PROCESADO
// Si no coincide, se responde RECHAZADO

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#define BACKLOG 8      // conexiones en espera
#define LINE_MAX 256   // tamaño máximo de la línea de cabecera
#define RECV_BUF 2048  // (no lo uso, pero lo dejo para pruebas)

// Cifrado César, lo tomé de Ejercicio_1_JAHA.c
void encryptCaesar(char *s, int shift) {
    shift %= 26;
    if (shift < 0) shift += 26;
    for (int i = 0; s[i] != '\0'; i++) {
        unsigned char c = s[i];
        if (isupper(c))      s[i] = ((c - 'A' + shift) % 26) + 'A';
        else if (islower(c)) s[i] = ((c - 'a' + shift) % 26) + 'a';
    }
}

static ssize_t read_line(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) break;      // cierre del otro lado
        if (r < 0)  return -1;  // error
        buf[n++] = c;
        if (c == '\n') break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

// lee exactamente n bytes del socket
static int read_nbytes(int fd, char *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r == 0) return -1;                
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return 0;
}

// envía todo el buffer 
static int send_all(int fd, const char *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(fd, buf + sent, n - sent, 0);
        if (w <= 0) { if (errno == EINTR) continue; return -1; }
        sent += (size_t)w;
    }
    return 0;
}

// Atender a un cliente
static void handle_client(int cfd, int listen_port) {
    char line[LINE_MAX];

    // cabecera:
    ssize_t ln = read_line(cfd, line, sizeof(line));
    if (ln <= 0) return;

    int porto = -1, shift = 0;
    size_t len = 0;
    if (sscanf(line, "PORTO=%d;SHIFT=%d;LEN=%zu", &porto, &shift, &len) != 3) {
        // si no se entiende la cabecera, se rechaza
        send_all(cfd, "RECHAZADO\n", 10);
        return;
    }

    // se lee exactamente LEN bytes de cuerpo
    char *body = (char*)malloc(len + 1);
    if (!body) return;
    if (read_nbytes(cfd, body, len) != 0) {
        free(body);
        return;
    }
    body[len] = '\0';

    // si PORTO coincide con el puerto, se cifra y se responde
    if (porto == listen_port) {
        encryptCaesar(body, shift);

        // lo muestro en la terminal del servidor 
        printf("[SERVER %d] Encrypted:\n%s\n", listen_port, body);

        if (send_all(cfd, "PROCESADO\n", 11) == 0) {
            send_all(cfd, body, strlen(body));
            send_all(cfd, "\n", 1); 
        }
    } else {
        // si no coincide, se imprime RECHAZADO
        send_all(cfd, "RECHAZADO\n", 10);
    }

    free(body);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }
    int listen_port = atoi(argv[1]);

    // socket de escucha
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(sfd); return 1;
    }
    if (listen(sfd, BACKLOG) < 0) {
        perror("listen"); close(sfd); return 1;
    }
    printf("[+] Server listening on port %d...\n", listen_port);

    for (;;) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr*)&cli, &clilen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        handle_client(cfd, listen_port);
        close(cfd);
    }

    close(sfd);
    return 0;
}
//finnnn
