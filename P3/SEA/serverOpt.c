#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/select.h>

#define TAM_BUFFER 1024
#define MAX_PUERTOS 3

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

void guardar_archivo_cifrado(const char *contenido, int puerto) {
    char nombre_archivo[50];
    sprintf(nombre_archivo, "file_%d_cesar.txt", puerto);
    
    FILE *archivo = fopen(nombre_archivo, "w");
    if (archivo != NULL) {
        fprintf(archivo, "%s", contenido);
        fclose(archivo);
        printf("[Puerto %d] Archivo recibido y cifrado como %s\n", puerto, nombre_archivo);
    } else {
        printf("[Puerto %d] Error al guardar archivo\n", puerto);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <PUERTO1> <PUERTO2> <PUERTO3>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int puertos[MAX_PUERTOS];
    int servidores_fd[MAX_PUERTOS];
    fd_set conj_read;
    int max_fd = 0;
    struct sockaddr_in direcciones[MAX_PUERTOS];

    printf("[Server] Iniciando servidor optimizado...\n");

    // Configuramos los puertos
    for (int i = 0; i < MAX_PUERTOS; i++) {
        puertos[i] = atoi(argv[i + 1]);
        
        // Creamos un socket
        if ((servidores_fd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error al crear socket");
            exit(EXIT_FAILURE);
        }

        // Decimos que si a reusar dirección
        int yes = 1;
        if (setsockopt(servidores_fd[i], SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            perror("Error en setsockopt");
            close(servidores_fd[i]);
            exit(EXIT_FAILURE);
        }

        // Configuramos dirección
        direcciones[i].sin_family = AF_INET;
        direcciones[i].sin_addr.s_addr = INADDR_ANY;
        direcciones[i].sin_port = htons(puertos[i]);

        // Enlazamos socket
        if (bind(servidores_fd[i], (struct sockaddr *)&direcciones[i], sizeof(direcciones[i])) < 0) {
            perror("Error al enlazar");
            close(servidores_fd[i]);
            exit(EXIT_FAILURE);
        }

        // Escuchamos las conexiones
        if (listen(servidores_fd[i], 3) < 0) {
            perror("Error al escuchar");
            close(servidores_fd[i]);
            exit(EXIT_FAILURE);
        }

        printf("[Server] Escuchando puerto %d\n", puertos[i]);
        if (servidores_fd[i] > max_fd) max_fd = servidores_fd[i];
    }

    max_fd++;
    printf("[Server] Todos los puertos configurados. Esperando conexiones...\n");

    while (1) {
        FD_ZERO(&conj_read);
        for (int i = 0; i < MAX_PUERTOS; i++) {
            FD_SET(servidores_fd[i], &conj_read);
        }

        // Esperamos a los sockets
        if (select(max_fd, &conj_read, NULL, NULL, NULL) < 0) {
            perror("Error en select");
            continue;
        }

        // Vemos cada socket
        for (int i = 0; i < MAX_PUERTOS; i++) {
            if (FD_ISSET(servidores_fd[i], &conj_read)) {
                socklen_t tam_direccion = sizeof(direcciones[i]);
                int nuevo_socket = accept(servidores_fd[i], (struct sockaddr *)&direcciones[i], &tam_direccion);
                
                if (nuevo_socket < 0) {
                    perror("Error al aceptar conexión");
                    continue;
                }

                char buffer[TAM_BUFFER] = {0};
                int bytes_recibidos = recv(nuevo_socket, buffer, TAM_BUFFER - 1, 0);
                
                if (bytes_recibidos > 0) {
                    buffer[bytes_recibidos] = '\0';
                    
                    int puerto_objetivo, desplazamiento;
                    sscanf(buffer, "%d %d", &puerto_objetivo, &desplazamiento);
                    
                    char *contenido_archivo = strchr(buffer, ' ');
                    if (contenido_archivo != NULL) {
                        contenido_archivo = strchr(contenido_archivo + 1, ' ');
                        if (contenido_archivo != NULL) contenido_archivo++;
                    }

                    if (puerto_objetivo == puertos[i] && contenido_archivo != NULL) {
                        cifrar_cesar(contenido_archivo, desplazamiento);
                        guardar_archivo_cifrado(contenido_archivo, puertos[i]);
                        send(nuevo_socket, "ARCHIVO CIFRADO RECIBIDO", 24, 0);
                    } else {
                        send(nuevo_socket, "RECHAZADO", 9, 0);
                    }
                }
                
                close(nuevo_socket);
            }
        }
    }

    for (int i = 0; i < MAX_PUERTOS; i++) {
        close(servidores_fd[i]);
    }
    
    return 0;
}