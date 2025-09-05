#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define PUERTO 5000

int main() {
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;
    char *mensaje = "¡Hola desde el servidor!\n";

    // Crear socket
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Configurar dirección
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(PUERTO);

    // Enlazar socket
    bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion));

    // Escuchar conexiones
    listen(servidor_fd, 3);

    // Aceptar conexión
    nuevo_socket = accept(servidor_fd, NULL, NULL);

    // Enviar mensaje
    send(nuevo_socket, mensaje, strlen(mensaje), 0);

    // Cerrar sockets
    close(nuevo_socket);
    close(servidor_fd);

    return 0;
}
