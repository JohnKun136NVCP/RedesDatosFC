/* Ejercicio 2, Práctica 3: server.c
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
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

//Espacio en memoria que vamos a reservar por defecto para procesar los mensajes.
#define BUFFER_SIZE 4096
// Tope de conexiones
#define BACKLOG 10

/**
 * La función para encriptar con cifrado César que ya teníamos.
 * @param text Texto a encriptar.
 * @param shift Desplazamiento.
 */
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    if (shift < 0) {
        shift += 26;
    }

    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

/**
 * Recibe un mensaje, lo encripta, lo guarda y manda un mensaje de resultado al cliente.
 * @param client_sock El descriptor del socket destinado a esta conexión.
 * @param port El puerto de la conexión.
 */
void handle_client(int client_sock, int port) {
    char buffer[BUFFER_SIZE] = {0};
    // Datos recibidos del cliente
    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("[SERVER %d] Client disconnected gracefully.\n", port);
        } else {
            perror("recv");
        }
        close(client_sock);
        return;
    }
    // Verificamos que efectivamente hallamos recibido un string.
    buffer[bytes_received] = '\0';
    // El cliente envía datos de la forma: "<mensaje_secreto> <desplazamiento>"
    // Así que tenemos que separar el mensaje entero recibido a partir del espacio '\0'
    char *last_space = strrchr(buffer, ' ');
    // Si no se cumple la condición anterior, mandamos un mensaje de error informando que el mensaje no es válido.
    if (last_space == NULL) {
        fprintf(stderr, "[SERVER %d] Invalid data format from client.\n", port);
        send(client_sock, "ACCESS DENIED: Invalid format", strlen("ACCESS DENIED: Invalid format"), 0);
        close(client_sock);
        return;
    }
    // Separamos el mensaje recibido en dos: el mensaje secreto que se va a encriptar, y el desplazamiento:
    *last_space = '\0';
    char *text_to_encrypt = buffer;
    int shift = atoi(last_space + 1);
    // Encriptamos:
    encryptCaesar(text_to_encrypt, shift);

    // Y ahora podemos guardarlo en un archivo local.
    char filename[256];
    int counter = 0;
    snprintf(filename, sizeof(filename), "file_%d_caesar.txt", port);
    // Si el archivo ya existe, le vamos a poner un sufijo con un número.
    while (access(filename, F_OK) == 0) {
        counter++;
        snprintf(filename, sizeof(filename), "file_%d_caesar_%d.txt", port, counter);
    }
    // Escribimos el archivo.
    FILE *output_file = fopen(filename, "w");
    if (output_file == NULL) {
        perror("fopen");
        send(client_sock, "ACCESS DENIED: Server error", strlen("ACCESS DENIED: Server error"), 0);
        close(client_sock);
        return;
    }
    fprintf(output_file, "%s", text_to_encrypt);
    fclose(output_file);

    // Finalmente le devolvemos un mensaje de éxito al cliente.
    send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
    printf("[SERVER %d] Archivo recibido y cifrado como: %s\n", port, filename);
    close(client_sock);
}

/**
 * Inicializamos un socket que escuche a un puerto en particular.
 * * @param port Puerto a escuchar.
 */
void start_listening(int port) {
    int listen_sock, client_sock;
    struct sockaddr_in serv_addr;
    // Creamos el socket
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    // Si llega una conexión, la atendemos y volvemos a escuchar otras
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    // Bind
    if (bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    // Nos ponemos a escuchar
    if (listen(listen_sock, BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("[SERVER %d] Listening...\n", port);
    fflush(stdout);

    // Bucle para procesar las peticiones indefinidamente
    while (1) {
        client_sock = accept(listen_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("accept");
            continue;
            // Cuando concluyamos de procesar una petición, regresamos a esperar otras.
        }

        // Creamos un nuevo proceso para seguir procesando peticiones.
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_sock);
        } else if (pid == 0) {
            // Creamos un nuevo proceso para atender más peticiones y cerramos el anterior.
            close(listen_sock);
            handle_client(client_sock, port);
            exit(EXIT_SUCCESS);
        } else {
            // En otro caso, cerramos el proceso padre
            close(client_sock);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }
}


int main(int argc, char *argv[]) {
    // Nos aseguramos de haber recibido un único argumento (el puerto)
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    // Verificamos que el puerto sea válido.
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        return 1;
    }
    // Inicializamos el socket
    start_listening(port);
    return 0;
}
