#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
// Cambio para OPT: agregado para select()
#include <sys/select.h>

#define BUFFER_SIZE 1024

// Cifrado César
static void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;    // los caracteres no alfabéticos se quedan sin cambio
        }
    }
}

// Lee una línea desde el socket
static ssize_t recv_line(int fd, char *line, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        char c; ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) { if (i == 0) return 0; break; }
        if (n < 0) return -1;
        line[i++] = c;
        if (c == '\n') break;
    }
    line[i] = '\0';
    return (ssize_t)i;
}

// Cambio para OPT: main ya no recibe argv, y escuchamos en 3 puertos fijos en una sola ejecución
int main(void) {
    const int PORTS[] = {49200, 49201, 49202};
    const int NPORTS = 3;
    int listeners[NPORTS];

    // Cambio para OPT: creamos 3 listeners (antes era 1, ahora bucle)
    for (int i = 0; i < NPORTS; ++i) {
        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == -1) { perror("[-] socket"); return 1; }

        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    #ifdef SO_REUSEPORT
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    #endif

        struct sockaddr_in server_addr = {0};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons((uint16_t)PORTS[i]);     // antes el PORT era único
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("[-] bind"); close(server_sock); return 1;
        }
        if (listen(server_sock, 8) < 0) { perror("[-] listen"); close(server_sock); return 1; }

        listeners[i] = server_sock;
        printf("[+] SERVER %d LISTENING...\n", PORTS[i]);
    }

    // Cambio para OPT: bucle principal con select() para aceptar en cualquiera de los 3 listeners
    for (;;) {
        fd_set rfds; FD_ZERO(&rfds);
        int maxfd = -1;
        for (int i = 0; i < NPORTS; ++i) {
            FD_SET(listeners[i], &rfds);
            if (listeners[i] > maxfd) maxfd = listeners[i];
        }

        int ready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i < NPORTS; ++i) {
            if (!FD_ISSET(listeners[i], &rfds)) continue;

            struct sockaddr_in client_addr; socklen_t addr_size = sizeof(client_addr);
            int client_sock = accept(listeners[i], (struct sockaddr *)&client_addr, &addr_size);
            if (client_sock < 0) { perror("[-] accept"); continue; }

            // Cambio para OPT: definimos PORT según el listener que aceptó
            int PORT = PORTS[i];
            printf("[+] CLIENT CONNECTED TO %d\n", PORT);


            char line[4096], filename[512] = {0};
            int target_port = -1, k = 0;

            if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "TARGET %d", &target_port) != 1) {
                printf("[-] Bad TARGET\n"); goto end_client;
            }
            if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "SHIFT %d", &k) != 1) {
                printf("[-] Bad SHIFT\n"); goto end_client;
            }
            if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "NAME %511[^\n]", filename) != 1) {
                printf("[-] Bad NAME\n"); goto end_client;
            }
            if (recv_line(client_sock, line, sizeof(line)) < 0) {
                printf("[-] Bad BLANK LINE\n"); goto end_client;
            }

            size_t cap = 8192, len = 0; char *content = (char *)malloc(cap);
            if (!content) { perror("[-] malloc"); goto end_client; }
            for (;;) {
                char buf[4096];
                ssize_t n = recv(client_sock, buf, sizeof(buf), 0);
                if (n == 0) break;
                if (n < 0) { perror("[-] recv"); free(content); content = NULL; break; }
                if (len + (size_t)n + 1 > cap) {
                    size_t newcap = cap * 2;
                    char *tmp = realloc(content, newcap);
                    if (!tmp) { free(content); content = NULL; break; }
                    content = tmp; cap = newcap;
                }
                memcpy(content + len, buf, (size_t)n); len += (size_t)n;
            }
            if (!content) goto end_client;
            content[len] = '\0';

            if (target_port == PORT) {
                encryptCaesar(content, k);

                const char *base = filename;
                const char *slash = strrchr(filename, '/');
                if (slash && *(slash + 1) != '\0') base = slash + 1;

                char base_no_ext[512];
                const char *dot = strrchr(base, '.');
                size_t blen = dot ? (size_t)(dot - base) : strlen(base);
                if (blen >= sizeof(base_no_ext)) blen = sizeof(base_no_ext) - 1;
                memcpy(base_no_ext, base, blen);
                base_no_ext[blen] = '\0';

                char formatted_name[600];

                snprintf(formatted_name, sizeof(formatted_name), "%s_%d_cesar.txt", base_no_ext, PORT);

                dprintf(client_sock, "OK %d\nNAME %s\n\n", PORT, formatted_name);
                send(client_sock, content, len, 0);

                printf("[+] SERVER %d: Request accepted.\n", PORT);
                printf("[+] SERVER %d: File received and encrypted.\n", PORT);
                if (len > 0) printf("\n%s\n", content);
            } else {
                dprintf(client_sock, "REJECT %d\n", PORT);
                printf("[-] SERVER %d: Request rejected (client requested port %d)\n", PORT, target_port);
            }
            free(content);

        // Cambio para OPT: reemplazamos 'done' por 'end_client'
        end_client:
            close(client_sock);   // cerramos solo al cliente, y mantenemos los listeners activos
        }
    }

    // Cambio para OPT: cerramos todos los listeners al salir del bucle principal
    for (int i = 0; i < NPORTS; ++i) close(listeners[i]);
    return 0;
}
