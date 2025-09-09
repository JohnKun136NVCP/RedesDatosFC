#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define TAM_BUFFER 1024

void cifrar_cesar(char *texto, int desplazamiento) {
    desplazamiento = desplazamiento % 26;
    for (int i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (isupper(c)) {
            texto[i] = ((c - 'A' + desplazamiento) % 26) + 'A';
        } else if (islower(c)) {
            texto[i] = ((c - 'a' + desplazamiento) % 26) + 'a';
        } else {
            texto[i] = c;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int puerto_servidor = atoi(argv[1]);
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;
    char buffer[TAM_BUFFER] = {0};

    // Creamos un socket
    if ((servidor_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configuramos dirección
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto_servidor);

    // Conectamos socket
    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error al enlazar");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchamos las conexiones
    if (listen(servidor_fd, 3) < 0) {
        perror("Error al escuchar");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en puerto %d...\n", puerto_servidor);

    while (1) {
        // Aceptamos la conexión
        socklen_t tam_direccion = sizeof(direccion);
        if ((nuevo_socket = accept(servidor_fd, (struct sockaddr *)&direccion, &tam_direccion)) < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        // Recibimos los datos del cliente
        int bytes_recibidos = recv(nuevo_socket, buffer, TAM_BUFFER - 1, 0);
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';

            // Extraer puerto objetivo y desplazamiento
            int puerto_objetivo, desplazamiento;
            sscanf(buffer, "%d %d", &puerto_objetivo, &desplazamiento);

            // Nos da el contenido del archivo
            char *contenido_archivo = strchr(buffer, ' ');
            if (contenido_archivo != NULL) {
                contenido_archivo = strchr(contenido_archivo + 1, ' ');
                if (contenido_archivo != NULL) contenido_archivo++;
            }

            // Vemos si el puerto objetivo coincide con el puerto del servidor
            if (puerto_objetivo == puerto_servidor && contenido_archivo != NULL) {
                cifrar_cesar(contenido_archivo, desplazamiento);
                printf("Archivo procesado (puerto %d): %s\n", puerto_servidor, contenido_archivo);
                send(nuevo_socket, "PROCESADO", 9, 0);
            } else {
                printf("Solicitud rechazada (puerto %d != %d)\n", puerto_objetivo, puerto_servidor);
                send(nuevo_socket, "RECHAZADO", 9, 0);
            }
        } else {
            send(nuevo_socket, "ERROR: Sin datos", 16, 0);
        }

        close(nuevo_socket);
    }

    close(servidor_fd);
    return 0;
}