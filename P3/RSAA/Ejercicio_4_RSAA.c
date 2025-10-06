/* Ejercicio 4, Práctica 3: clientMulti.c
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

/**
 * Función para manejar cada conexión individual al server.
 * @param server_ip IP del servidor.
 * @param port Puerto de conexión.
 * @param shift Desplazamiento del encriptado César.
 * @param filepath El archivo de texto que se va a mandar.
 */
void connect_and_send(const char *server_ip, int port, int shift, const char *filepath) {
    FILE *secret_file = fopen(filepath, "r");
    if (secret_file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filepath);
        printf("SERVER RESPONSE %d: Rejected\n", port);
        return;
    }

    // Medimos el tamaño en bytes del archivo y lo cargamos en un bufer en memoria.
    fseek(secret_file, 0, SEEK_END);
    long file_size = ftell(secret_file);
    rewind(secret_file);
    char *secret_file_content = malloc(file_size + 1);
    if (secret_file_content == NULL) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        fclose(secret_file);
        return;
    }
    size_t bytes_read = fread(secret_file_content, 1, file_size, secret_file);
    if (bytes_read != file_size) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        fclose(secret_file);
        free(secret_file_content);
        return;
    }
    // El contenido del archivo debe ser un string, nos senrcioramos de esto colocando un espacio en blanco al final.
    secret_file_content[file_size] = '\0';
    fclose(secret_file);

    // Inicializamos el socket para intercambiar información con el servidor.
    int client_sock;
    struct sockaddr_in serv_addr;
    char server_response_buffer[BUFFER_SIZE] = {0};
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(secret_file_content);
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    // Usamos la IP pasada como argumento.
    if(inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(secret_file_content);
        close(client_sock);
        return;
    }
    // Nos conectamos al servidor
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        close(client_sock);
        free(secret_file_content);
        return;
    }
    // Si la conexión fué exitosa, la rutina de la función continua.
    // Ahora localizamos el mensaje en memoria y lo enviamos junto con el desplazamiento en un solo mensaje.
    char *mensaje = malloc(file_size + 16);
    if (mensaje == NULL) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(secret_file_content);
        close(client_sock);
        return;
    }
    snprintf(mensaje, file_size + 16, "%s %d", secret_file_content, shift);
    free(secret_file_content);
    if (send(client_sock, mensaje, strlen(mensaje), 0) < 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        free(mensaje);
        close(client_sock);
        return;
    }
    free(mensaje);

    // Esperamos respuesta del servidor.
    int bytes = recv(client_sock, server_response_buffer, sizeof(server_response_buffer) - 1, 0);
    if (bytes <= 0) {
        printf("SERVER RESPONSE %d: Rejected\n", port);
        close(client_sock);
        return;
    }
    server_response_buffer[bytes] = '\0';
    if (strstr(server_response_buffer, "ACCESS GRANTED") != NULL) {
        printf("SERVER RESPONSE %d: File received and encrypted\n", port);
    } else {
        printf("SERVER RESPONSE %d: Rejected\n", port);
    }
    close(client_sock);
}


int main(int argc, char *argv[]) {
    // He decidido que la forma de mandar parámetros desde el cliente sea la siguiente:
    // ./client <ip> <puerto> <desplazamiento> <archivo> ... <puerto_n> <desplazamiento_n> <archivo_n>
    // Es decir, la IP siempre va primero y después siguen tercias de puerto, desplazamiento y archivo a mandar.
    // Como mínimo, se esperan cuatro argumentos, así que podemos hacer una operación módulo para verificar
    // que el número de argumentos recibidos sea el correcto.
    if (argc < 5 || (argc - 2) % 3 != 0) {
        fprintf(stderr, "Usage: %s <server_ip> <port1> <shift1> <file1> [<port2> <shift2> <file2> ...]\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];

    // Si la verificación de argumentos fué correcta, iteramos en bucle para procesar cada tercia:
    for (int i = 2; i < argc; i += 3) {
        int port = atoi(argv[i]);
        int shift = atoi(argv[i + 1]);
        char *filepath = argv[i + 2];

        connect_and_send(server_ip, port, shift, filepath);
    }

    return 0;
}
