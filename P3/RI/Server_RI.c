#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define COLA 5
#define TAM_BUFFER (1<<20)

// Aplica el cifrado César
void cifrado_cesar(char *texto, int desplazamiento) {
    int k = desplazamiento % 26;   // normalizamos a 0..25
    if (k < 0) k += 26;            // evitamos valores negativos

    for (int i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (c >= 'A' && c <= 'Z') {
            texto[i] = (char)('A' + (c - 'A' + k) % 26);
        } else if (c >= 'a' && c <= 'z') {
            texto[i] = (char)('a' + (c - 'a' + k) % 26);
        }

    }
}

// Recibe todo lo que envía el cliente hasta que cierre su escritura o se llene el buffer.
// Devuelve bytes recibidos o -1 en error.
ssize_t recibir_todo(int fd, char *buffer, size_t capacidad) {
    size_t recibido = 0;
    while (recibido + 1 < capacidad) {
        ssize_t r = recv(fd, buffer + recibido, capacidad - 1 - recibido, 0);
        if (r > 0) { recibido += (size_t)r; continue; }
        if (r == 0) break;                         // el cliente terminó de enviar
        if (errno == EINTR) continue;              // reintentar si fue interrumpido
        return -1;
    }
    buffer[recibido] = '\0';                       // terminamos como string
    return (ssize_t)recibido;
}

// Acepta solo los puertos definidos
int puerto_valido(int p) {
    return (p == 49200 || p == 49201 || p == 49202);
}

int main(int argc, char *argv[]) {
    // Validación de argumentos
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    int puerto_escucha = atoi(argv[1]);
    if (!puerto_valido(puerto_escucha)) {
        printf("Solo se permiten los puertos 49200, 49201 o 49202\n");
        return 1;
    }

    // Crear socket TCP IPv4
    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor < 0) {
        perror("Error al crear socket");
        return 1;
    }

    // Permitir reusar el puerto
    int opcion = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

    // Asociar el socket a puerto_escucha
    struct sockaddr_in direccion;
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;                 // cualquier interfaz
    direccion.sin_port = htons((uint16_t)puerto_escucha);

    if (bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind");
        close(servidor);
        return 1;
    }

    // Poner el socket en modo escucha
    if (listen(servidor, COLA) < 0) {
        perror("Error en listen");
        close(servidor);
        return 1;
    }

    printf("[SERVIDOR %d] Esperando conexiones...\n", puerto_escucha);

    // Reservar buffer general
    char *buffer = malloc(TAM_BUFFER);
    if (!buffer) {
        printf("No hay memoria suficiente\n");
        close(servidor);
        return 1;
    }

    // Bucle principal: aceptar y atender cliente
    while (1) {
        struct sockaddr_in cliente;
        socklen_t tam_cliente = sizeof(cliente);
        int socket_cliente = accept(servidor, (struct sockaddr *)&cliente, &tam_cliente);
        if (socket_cliente < 0) {
            perror("Error en accept");
            continue;
        }

        printf("[SERVIDOR %d] Cliente conectado desde %s:%d\n",
               puerto_escucha, inet_ntoa(cliente.sin_addr), ntohs(cliente.sin_port));

        // Recibir todo el mensaje del cliente
        ssize_t n = recibir_todo(socket_cliente, buffer, TAM_BUFFER);
        if (n <= 0) {                 // nada recibido
            close(socket_cliente);
            continue;
        }

        // Parseo del protocolo:
        //    PORT X\n
        //    SHIFT Y\n
        //    contenido
        int puerto_objetivo = -1, desplazamiento = 0;
        char *p = buffer;
        char *linea1 = strsep(&p, "\n");
        char *linea2 = strsep(&p, "\n");
        strsep(&p, "\n");
        char *contenido = p ? p : (char *)"";

        if (linea1) sscanf(linea1, "PORT %d", &puerto_objetivo);
        if (linea2) sscanf(linea2, "SHIFT %d", &desplazamiento);

        // Verificar si el puerto objetivo coincide con el del servidor
        if (puerto_objetivo == puerto_escucha) {
            printf("[SERVIDOR %d] Solicitud aceptada\n", puerto_escucha);

            // Duplicamos el texto para cifrar sin tocar el original
            char *texto_copia = strdup(contenido);
            if (!texto_copia) { close(socket_cliente); continue; }

            // Cifrar y mostrar el resultado en la consola
            cifrado_cesar(texto_copia, desplazamiento);
            printf("[SERVIDOR %d] Texto cifrado:\n", puerto_escucha);
            puts(texto_copia);

            free(texto_copia);
        } else {
            // Por si se rechaza
            printf("[SERVIDOR %d] Solicitud rechazada (puerto objetivo %d)\n",
                   puerto_escucha, puerto_objetivo);
        }

        // Cerrar la conexión con el cliente y volver a aceptar otro
        close(socket_cliente);
    }

    free(buffer);
    close(servidor);
    return 0;
}