#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PUERTOS 3
int puertos[PUERTOS] = {49200, 49201, 49202};

void cifradoCesar(char *texto, int desplazamiento) {
    for (int i = 0; texto[i] != '\0'; i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z')
            texto[i] = 'a' + (texto[i] - 'a' + desplazamiento) % 26;
        else if (texto[i] >= 'A' && texto[i] <= 'Z')
            texto[i] = 'A' + (texto[i] - 'A' + desplazamiento) % 26;
    }
}

int main() {
    int sockets[PUERTOS], maxfd, cliente;
    struct sockaddr_in servidor, cliente_addr;
    socklen_t cliente_len;
    fd_set readfds;

    // Crear sockets para cada puerto
    for (int i = 0; i < PUERTOS; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i] < 0) {
            perror("Error creando socket");
            exit(1);
        }

        servidor.sin_family = AF_INET;
        servidor.sin_addr.s_addr = INADDR_ANY;
        servidor.sin_port = htons(puertos[i]);

        if (bind(sockets[i], (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
            perror("Error en bind");
            exit(1);
        }

        listen(sockets[i], 5);
        printf("Servidor escuchando en puerto %d...\n", puertos[i]);
    }

    while (1) {
        FD_ZERO(&readfds);
        maxfd = 0;

        // Agregar sockets al conjunto
        for (int i = 0; i < PUERTOS; i++) {
            FD_SET(sockets[i], &readfds);
            if (sockets[i] > maxfd) maxfd = sockets[i];
        }

        // Esperar conexiones en cualquiera
        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        for (int i = 0; i < PUERTOS; i++) {
            if (FD_ISSET(sockets[i], &readfds)) {
                cliente_len = sizeof(cliente_addr);
                cliente = accept(sockets[i], (struct sockaddr *)&cliente_addr, &cliente_len);
                if (cliente < 0) {
                    perror("Error en accept");
                    continue;
                }

                char buffer[1024];
                int desplazamiento, puertoObjetivo;
                recv(cliente, &puertoObjetivo, sizeof(int), 0);
                recv(cliente, &desplazamiento, sizeof(int), 0);
                recv(cliente, buffer, sizeof(buffer), 0);

                printf("Conexión recibida en puerto %d, objetivo %d\n", puertos[i], puertoObjetivo);

                if (puertoObjetivo == puertos[i]) {
                    cifradoCesar(buffer, desplazamiento);
                    send(cliente, "Procesado", 10, 0);
                    printf("Archivo procesado: %s\n", buffer);
                } else {
                    send(cliente, "Rechazado", 10, 0);
                }

                close(cliente);
            }
        }
    }

    return 0;
}
