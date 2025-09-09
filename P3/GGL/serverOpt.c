#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define NUM_PORTS 3

int ports[NUM_PORTS] = {49200, 49201, 49202};

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

void save_encrypted_file(char *original_text, char *encrypted_text, int port, char *client_ip) {
    // Crear nombre de archivo con formato: mensaje_49200_cesar.txt
    char filename[256];
    snprintf(filename, sizeof(filename), "mensaje_%d_cesar.txt", port);
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("[-] Error al crear archivo");
        return;
    }

    //fprintf(fp, "Cliente: %s", client_ip);
    //fprintf(fp, " Puerto: %d\n", port);
    //fprintf(fp, "Texto original: %s\n", original_text);
    fprintf(fp, "Texto cifrado: %s\n", encrypted_text);
    fclose(fp);
    printf("[+] Archivo guardado: %s\n", filename);
}

void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);

    // Obtener IP del cliente
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    getpeername(client_sock, (struct sockaddr *)&client_addr, &client_len);
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    char buffer[BUFFER_SIZE] = {0};
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        perror("[-] Error al recibir datos");
        close(client_sock);
        pthread_exit(NULL);
    }
    buffer[bytes] = '\0';

    int target_port, shift;
    char text[BUFFER_SIZE];
    char original_text[BUFFER_SIZE];
    sscanf(buffer, "%d %d %[^\n]", &target_port, &shift, text);
    strcpy(original_text, text); // Guardar copia del texto original

    // Obtener el puerto real del socket
    struct sockaddr_in server_addr;
    socklen_t len = sizeof(server_addr);
    getsockname(client_sock, (struct sockaddr *)&server_addr, &len);
    int actual_port = ntohs(server_addr.sin_port);

   // printf("\n[+] Cliente conectado: %s\n", client_ip);
    printf("------------------------------\n");
    printf("[+] Puerto conexión: %d", actual_port);
    printf(" [+] Puerto solicitado: %d\n", target_port);
   // printf("[+] Desplazamiento: %d\n", shift);
   // printf("[+] Texto recibido: %s\n", text);

    if (target_port == actual_port) {
        encryptCaesar(text, shift);
        // printf("[+] Texto cifrado: %s\n", text);
        // Guardar archivo en el servidor
        save_encrypted_file(original_text, text, actual_port, client_ip);
        // Enviar respuesta al cliente
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "PROCESADO: Archivo guardado como mensaje_%d_cesar.txt", actual_port);
        send(client_sock, response, strlen(response), 0);
        printf("[+] Respuesta enviada al cliente: %s\n", response);
    } else {
        send(client_sock, "RECHAZADO", 9, 0);
        printf("[-] Puerto no coincide. Rechazado.\n");
    }

    close(client_sock);
    pthread_exit(NULL);
}

void *listen_port(void *arg) {
    int port = *((int *)arg);
    free(arg);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-] Error al crear socket");
        pthread_exit(NULL);
    }

    // Permitir reuso de puerto
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        pthread_exit(NULL);
    }

    if (listen(server_sock, 10) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        pthread_exit(NULL);
    }

    printf("[+] Servidor escuchando en puerto %d...\n", port);
    while (1) {
        int *client_sock = malloc(sizeof(int));
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*client_sock < 0) {
            perror("[-] Error en accept");
            free(client_sock);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    close(server_sock);
    pthread_exit(NULL);
}

int main() {
    printf("[+] Iniciando servidor optimizado en múltiples puertos...\n");
    printf("[+] Los archivos cifrados se guardarán en el servidor\n");

    pthread_t threads[NUM_PORTS];

    // Crear un hilo por cada puerto
    for (int i = 0; i < NUM_PORTS; i++) {
        int *port = malloc(sizeof(int));
        *port = ports[i];
        if (pthread_create(&threads[i], NULL, listen_port, port) != 0) {
            perror("[-] Error al crear hilo");
            free(port);
        }
    }

    printf("[+] Servidor ejecutándose en puertos: 49200, 49201, 49202\n");
    printf("[+] Presiona Ctrl+C para detener el servidor\n");
    // Esperar a que todos los hilos terminen (nunca deberían terminar)
    for (int i = 0; i < NUM_PORTS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}