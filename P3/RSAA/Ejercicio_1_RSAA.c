/* Ejercicio 1, Práctica 3: client.c
 * Elaborado por Alejandro Axel Rodríguez Sánchez
 * ahexo@ciencias.unam.mx
 *
 * Facultad de Ciencias UNAM
 * Redes de computadoras
 * Grupo 7006
 * Semestre 2026-1
 * 2025-09-07
 *
 * Profesor:
 * Luis Enrique Serrano Gutiérrez
 * Ayudante de laboratorio:
 * Juan Angeles Hernández
 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Espacio en memoria que vamos a reservar por defecto para procesar los mensajes.
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    // Procesamos y parseamos los argumentos.
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <shift> <file>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int shift = atoi(argv[2]);
    char *filepath = argv[3];

    // Abrimos el archivo, si no se puede, devolvemos error.
    FILE *secret_file = fopen(filepath, "r");
    if (secret_file == NULL) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        return 1;
    }

    // Calculamos cuantos bytes necesitaremos para cargar el archivo en memoria en un buffer:
    fseek(secret_file, 0, SEEK_END);
    long file_size = ftell(secret_file);
    rewind(secret_file);
    // Reservamos memoria:
    char *secret_file_content = malloc(file_size + 1);
    if (secret_file_content == NULL) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        fclose(secret_file);
        return 1;
    }
    // Volcamos el contenido del archivo en el buffer y lo cerramos:
    size_t bytes_read = fread(secret_file_content, 1, file_size, secret_file);
    if (bytes_read != file_size) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        fclose(secret_file);
        free(secret_file_content);
        return 1;
    }
    secret_file_content[file_size] = '\0';
    fclose(secret_file);

    // Inicializamos un socket apuntando a la IP del servidor.
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char *server_ip = "192.168.1.76";
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(secret_file_content);
        return 1;
    }
    // Le asignamos el puerto que obtuvimos al invocar el programa.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Intentamos conectarnos, si fallamos, liberamos memoria y terminamos.
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        close(client_sock);
        free(secret_file_content);
        return 1;
    }
    char *mensaje = malloc(file_size + 16);
    if (mensaje == NULL) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(secret_file_content);
        close(client_sock);
        return 1;
    }
    // Si logramos conectarnos, cargamos el contenido del archivo y el shift para mandarlos.
    snprintf(mensaje, file_size + 16, "%s %d", secret_file_content, shift);
    free(secret_file_content);
    if (send(client_sock, mensaje, strlen(mensaje), 0) < 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(mensaje);
        close(client_sock);
        return 1;
    }
    free(mensaje);

    // Esperamos una respuesta del servidor.
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        close(client_sock);
        return 1;
    }
    buffer[bytes] = '\0';

    // Imprimimos la respuesta del servidor.
    if (strstr(buffer, "ACCESS GRANTED") != NULL) {
        printf("SERVER RESPONSE %d: File received and encrypted\n", port);
    } else {
        printf("SERVER RESPONSE %d: Rejected\n", port);
    }
    close(client_sock);
    return 0;
}

