#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define TAM_BUFFER 1024

int main(int argc, char *argv[]) {
    if (argc < 5 || (argc - 2) % 3 != 0) {
        printf("Uso: %s <SERVIDOR_IP> <PUERTO1> <DESPLAZAMIENTO1> <ARCHIVO1> [<PUERTO2> <DESPLAZAMIENTO2> <ARCHIVO2> ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *servidor_ip = argv[1];
    int num_conexiones = (argc - 2) / 3;

    printf("[Client] Iniciando cliente multiple para %d conexiones\n", num_conexiones);

    for (int i = 0; i < num_conexiones; i++) {
        int puerto = atoi(argv[2 + i * 3]);
        int desplazamiento = atoi(argv[3 + i * 3]);
        char *nombre_archivo = argv[4 + i * 3];
        int puerto_objetivo = 49200;  // Puerto  

        int cliente_fd;
        struct sockaddr_in direccion_servidor;
        char buffer[TAM_BUFFER] = {0};

        // Creamos el socket
        if ((cliente_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error al crear socket");
            continue;
        }

        // Configuramos la dirección del servidor
        direccion_servidor.sin_family = AF_INET;
        direccion_servidor.sin_port = htons(puerto);
        if (inet_pton(AF_INET, servidor_ip, &direccion_servidor.sin_addr) <= 0) {
            perror("Dirección inválida");
            close(cliente_fd);
            continue;
        }

        // Conectamos al servidor
        if (connect(cliente_fd, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
            perror("Conexión fallida");
            close(cliente_fd);
            continue;
        }

        // Leemos un archivo
        FILE *archivo = fopen(nombre_archivo, "r");
        if (archivo == NULL) {
            perror("Error al abrir archivo");
            close(cliente_fd);
            continue;
        }

        fseek(archivo, 0, SEEK_END);
        long tam_archivo = ftell(archivo);
        fseek(archivo, 0, SEEK_SET);

        char *contenido = malloc(tam_archivo + 1);
        fread(contenido, 1, tam_archivo, archivo);
        contenido[tam_archivo] = '\0';
        fclose(archivo);

        // Enviamos los datos
        sprintf(buffer, "%d %d %s", puerto_objetivo, desplazamiento, contenido);
        send(cliente_fd, buffer, strlen(buffer), 0);

        printf("[Client] Enviando a puerto %d: archivo=%s, desplazamiento=%d\n", 
               puerto, nombre_archivo, desplazamiento);

        // Recibimos una respuesta
        int bytes_recibidos = recv(cliente_fd, buffer, TAM_BUFFER - 1, 0);
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
            printf("[Cliente] Puerto %d: %s\n", puerto, buffer);
        } else {
            printf("[Cliente] Puerto %d: No se recibió respuesta\n", puerto);
        }

        free(contenido);
        close(cliente_fd);
        
        sleep(1);
    }

    printf("[Client] Todas las conexiones completadas\n");
    return 0;
}