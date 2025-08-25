#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PUERTO 5000

int main() {
    int cliente_fd;
    struct sockaddr_in direccion;
    char buffer[1024] = {0};

    // Crear socket
    cliente_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Configurar dirección
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(PUERTO);
    inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

    // Conectar al servidor
    connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion));

    // Leer mensaje
    read(cliente_fd, buffer, sizeof(buffer));
    printf("Mensaje del servidor: %s", buffer);

    // Cerrar socket
    close(cliente_fd);

    return 0;
}
