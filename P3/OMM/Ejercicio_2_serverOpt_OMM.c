#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define NUM_PORTS   3
const int ports[NUM_PORTS] = {49200, 49201, 49202};

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    if (shift < 0) shift += 26;
    for (int i = 0; text[i]; ++i) {
        char c = text[i];
        if (isupper(c)) text[i] = 'A' + (c - 'A' + shift) % 26;
        else if (islower(c)) text[i] = 'a' + (c - 'a' + shift) % 26;
    }
}

int main(void) {
    int server_fds[NUM_PORTS];
    fd_set read_fds;
    int max_fd = 0;
    struct sockaddr_in addr[NUM_PORTS];

    /* crea, configura y enlaza los tres sockets */
    for (int i = 0; i < NUM_PORTS; ++i) {
        server_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fds[i] == -1) { perror("socket"); exit(EXIT_FAILURE); }

        int opt = 1;
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        addr[i].sin_family      = AF_INET;
        addr[i].sin_addr.s_addr = INADDR_ANY;
        addr[i].sin_port        = htons(ports[i]);

        if (bind(server_fds[i], (struct sockaddr *)&addr[i], sizeof(addr[i])) == -1 ||
            listen(server_fds[i], 10) == -1) {
            perror("bind/listen"); exit(EXIT_FAILURE);
        }

        printf("[Servidor] Escuchando puerto %d\n", ports[i]);
        if (server_fds[i] > max_fd) max_fd = server_fds[i];
    }

    puts("Servidor optimizado ejecutándose. Presiona Ctrl+C para salir.");

    while (1) {
        FD_ZERO(&read_fds);
        for (int i = 0; i < NUM_PORTS; ++i) FD_SET(server_fds[i], &read_fds);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) { perror("select"); continue; }

        for (int i = 0; i < NUM_PORTS; ++i) {
            if (!FD_ISSET(server_fds[i], &read_fds)) continue;

            struct sockaddr_in client_addr;
            socklen_t cli_len = sizeof(client_addr);
            int cli = accept(server_fds[i], (struct sockaddr *)&client_addr, &cli_len);
            if (cli < 0) { perror("accept"); continue; }

            char buffer[BUFFER_SIZE] = {0};
            ssize_t n = recv(cli, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) { close(cli); continue; }
            buffer[n] = '\0';

            char *nl = strchr(buffer, '\n');
            if (!nl) { send(cli, "ERROR\n", 6, 0); close(cli); continue; }
            *nl = '\0';

            int target_port, shift;
            if (sscanf(buffer, "%d %d", &target_port, &shift) != 2) {
                send(cli, "ERROR\n", 6, 0); close(cli); continue;
            }
            char *content = nl + 1;

            printf("[Puerto %d] Solicitud – puerto objetivo: %d, desplazamiento: %d\n",
                   ports[i], target_port, shift);

            if (target_port == ports[i]) {
                encryptCaesar(content, shift);

                char output[256];
                snprintf(output, sizeof(output), "%.200s_%d_cesar.txt", "texto", ports[i]);

                FILE *fp = fopen(output, "w");
                if (fp) {
                    fputs(content, fp);
                    fclose(fp);
                    printf("[Puerto %d] Archivo cifrado guardado como %s\n", ports[i], output);
                    send(cli, "ARCHIVO CIFRADO RECIBIDO\n", 25, 0);
                } else {
                    perror("fopen");
                    send(cli, "ERROR\n", 6, 0);
                }
            } else {
                printf("[Puerto %d] Rechazado (puerto no coincide)\n", ports[i]);
                send(cli, "RECHAZADO\n", 10, 0);
            }
            close(cli);
        }
    }
    return 0;
}
