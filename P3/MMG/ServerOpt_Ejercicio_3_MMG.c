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
#include <pthread.h>

#define BUFFER_SIZE 8192
#define PUERTOS_SERV 3
static int puertos_servidor[PUERTOS_SERV] = {49200, 49201, 49202};

// ======= Función de Cifrado César de practica 2 =======
void encryptCesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isupper(c)) {
            // Caso letras mayúsculas
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
             // Caso letras minúsculas
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            // Cualquier otro carácter se deja igual
            text[i] = (char)c;
        }
    }
}

// Estructura para pasar argumentos al hilo de cada puerto
typedef struct {
    int puerto;
} thread_args_t;

// ======= Función que atiende conexiones en un puerto =======
void *handle_port(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    int puerto = args->puerto;
    free(args); // ya no se ocupa la memoria

    int sockfd = -1, newsockfd = -1;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];

    // Se crea el socket del servidor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        pthread_exit(NULL);
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(puerto);

    // Asociamos el socket al puerto correspondiente
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        pthread_exit(NULL);
    }

    // Lo dejamos en modo escucha
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        pthread_exit(NULL);
    }

    printf("[*]SERVIDOR en puerto %d LISTO y ESCUCHANDO...\n", puerto);
    fflush(stdout);

    clilen = sizeof(cli_addr);
    while (1) {
        // Aceptamos la conexión de un cliente
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            close(newsockfd);
            continue;
        }

        int puerto_obj = 0, shift = 0;
        char texto[BUFFER_SIZE];
        texto[0] = '\0';

        // Se espera el formato: <PUERTO> <DESPLAZAMIENTO> <TEXTO>
        if (sscanf(buffer, "%d %d %[^\n]", &puerto_obj, &shift, texto) < 2) {
            const char *errorFormato = "Formato inválido (esperado: <PUERTO> <DESPLAZAMIENTO> <TEXTO>)\n";
            write(newsockfd, errorFormato, strlen(errorFormato));
            close(newsockfd);
            continue;
        }

        // Si el puerto no coincide, se rechaza la petición
        if (puerto_obj != puerto) {
            printf("[*]SERVIDOR %d -> Solicitud rechazada (cliente pidió puerto %d)\n",
                   puerto, puerto_obj);
            char rechazo[128];
            snprintf(rechazo, sizeof(rechazo), "Rechazado por servidor en puerto %d\n", puerto);
            write(newsockfd, rechazo, strlen(rechazo));
            close(newsockfd);
            continue;
        }

        // Procesamos la solicitud: cifrar con César
        encryptCesar(texto, shift);

        // Guardamos la salida en un txt
        char nombre_archivo[128];
        snprintf(nombre_archivo, sizeof(nombre_archivo), "cifrado_cesar_%d.txt", puerto);

        FILE *fp = fopen(nombre_archivo, "w");
        if (fp != NULL) {
            fprintf(fp, "%s", texto);
            fclose(fp);        
            printf("[*]SERVIDOR %d -> Archivo recibido y cifrado, guardado como: %s\n",
                   puerto, nombre_archivo);
        } else {
            perror("[-]Error al crear o escribir en el archivo de salida");
        }

        // Enviamos respuesta al cliente
        char respuesta[BUFFER_SIZE];
        snprintf(respuesta, sizeof(respuesta), "Archivo procesado exitosamente \n");
        write(newsockfd, respuesta, strlen(respuesta));

        close(newsockfd); // cerramos la conexión con ese cliente
    }

    close(sockfd);
    pthread_exit(NULL);
}

// ======= Servidor Principal (crea los hilos de cada puerto) =======
int main() {
    pthread_t threads[PUERTOS_SERV];

    for (int i = 0; i < PUERTOS_SERV; i++) {
        int puerto = puertos_servidor[i];

        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (args == NULL) {
            perror("malloc");
            continue;
        }
        args->puerto = puerto;

        // Se lanza un hilo para atender cada puerto
        if (pthread_create(&threads[i], NULL, handle_port, (void *)args) != 0) {
            perror("pthread_create");
            free(args);
            continue;
        }
    }

    // Se espera a que terminen los hilos
    for (int i = 0; i < PUERTOS_SERV; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
