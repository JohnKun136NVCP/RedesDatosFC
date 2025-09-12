#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUF 8192

// Cifrado César
void caesar(char *s, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; s[i] != '\0'; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') {
            s[i] = 'A' + (c - 'A' + shift) % 26;
        } else if (c >= 'a' && c <= 'z') {
            s[i] = 'a' + (c - 'a' + shift) % 26;
        }
    }
}

int recv_line(int sock, char *line, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1;
        line[i++] = c;
        if (c == '\n') break;
    }
    line[i] = '\0';
    return i;
}

// Envía todo el buffer
int send_all(int sock, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        printf("Ejemplo: %s 49200\n", argv[0]);
        return 1;
    }

    int myport = atoi(argv[1]);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)myport);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return 1; }
    if (listen(s, 5) < 0) { perror("listen"); close(s); return 1; }

    printf("[*][SERVER %d] LISTENING...\n", myport);

    while (1) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { perror("accept"); continue; }

        // Mensaje al aceptar conexión
        printf("[*][SERVER %d] Request accepted...\n", myport);

        char header[128];
        if (recv_line(c, header, sizeof(header)) <= 0) { close(c); continue; }

        int target = 0, shift = 0;
        long size = 0;
        if (sscanf(header, "PORT %d SHIFT %d SIZE %ld", &target, &shift, &size) != 3 || size < 0) {
            const char *rej = "REJECT\n";
            send_all(c, rej, (int)strlen(rej));
            close(c);
            continue;
        }

        // Recibir el archivo
        char *data = (char*)malloc((size_t)size + 1);
        if (!data) { close(c); continue; }
        long got = 0;
        while (got < size) {
            int n = recv(c, data + got, (int)(size - got), 0);
            if (n <= 0) { break; }
            got += n;
        }
        data[got] = '\0';

        if (target == myport) {
            // Procesar: cifrar y responder
            caesar(data, shift);

            const char *ok = "PROCESSED\n";
            send_all(c, ok, (int)strlen(ok));
            if (got > 0) send_all(c, data, (int)got);

            // Mostrar texto cifrado
            printf("[*][SERVER %d] File received and encrypted:\n", myport);
            puts(data);
        } else {
            const char *rej = "REJECT\n";
            send_all(c, rej, (int)strlen(rej));
            // Cambio de formato de rechazo
            printf("[*][SERVER %d] REJECT (target=%d)\n", myport, target);
        }

        free(data);
        close(c);
    }

    close(s);
    return 0;
}

