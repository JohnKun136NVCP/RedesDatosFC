//Server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 2048

// Función de cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        // Otros caracteres no cambian
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int puerto_escucha = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[-] Error al crear el socket");
        exit(1);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto_escucha);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Enlazar socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error al hacer bind");
        close(server_fd);
        exit(1);
    }

    // Escuchar conexiones
    if (listen(server_fd, 5) < 0) {
        perror("[-] Error al escuchar");
        close(server_fd);
        exit(1);
    }

    printf("[+] Servidor escuchando en puerto %d...\n", puerto_escucha);

    while (1) {
        // Aceptar conexión
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[-] Error al aceptar conexión");
            continue;
        }

        printf("[+] Cliente conectado\n");

        // Recibir datos del cliente
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("[-] Error al recibir datos");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        // Extraer datos: <puerto_objetivo> <shift> <contenido>
        int puerto_objetivo, shift;
        char texto[BUFFER_SIZE];

        sscanf(buffer, "%d %d %[^\t\n]", &puerto_objetivo, &shift, texto);

        printf("[+] Puerto objetivo recibido: %d\n", puerto_objetivo);
        printf("[+] Shift: %d\n", shift);
        printf("[+] Texto recibido: %s\n", texto);

        if (puerto_objetivo == puerto_escucha) {
            encryptCaesar(texto, shift);
            char respuesta[BUFFER_SIZE];
            snprintf(respuesta, sizeof(respuesta), "Procesado: %s", texto);
            send(client_fd, respuesta, strlen(respuesta), 0);
            printf("[+] Texto cifrado enviado\n");
        } else {
            char *rechazo = "Rechazado";
            send(client_fd, rechazo, strlen(rechazo), 0);
            printf("[-] Solicitud rechazada (puerto no coincide)\n");
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
