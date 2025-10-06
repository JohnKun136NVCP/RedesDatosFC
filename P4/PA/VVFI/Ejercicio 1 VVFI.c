//Server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 2048  // Tamaño máximo del buffer para enviar/recibir datos
void encryptCaesar(char *text, int shift) {
    shift = shift % 26; // Asegura que el desplazamiento sea dentro del alfabeto
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int puerto_escucha = atoi(argv[1]);  // Puerto donde escuchará el servidor
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("[-] Error al crear socket"); exit(1); }

    // Permite reutilizar rápidamente el puerto si se reinicia el servidor
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto_escucha);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error al hacer bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("[-] Error al escuchar");
        close(server_fd);
        exit(1);
    }

    printf("[+] Servidor escuchando en puerto %d...\n", puerto_escucha);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[-] Error al aceptar conexión");
            continue;
        }

        printf("[+] Cliente conectado desde %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Recibir datos del cliente
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("[-] Error al recibir datos");
            close(client_fd);
            continue;
        }
        buffer[bytes_received] = '\0';  // Asegura que sea una cadena válida

        // Separar puerto, shift y texto del mensaje
        int puerto_objetivo, shift;
        char texto[BUFFER_SIZE];
        sscanf(buffer, "%d %d %[^\t\n]", &puerto_objetivo, &shift, texto);

        if (puerto_objetivo == puerto_escucha) {
            encryptCaesar(texto, shift);  // Cifrar mensaje
            char respuesta[BUFFER_SIZE];
            snprintf(respuesta, sizeof(respuesta), "Procesado: %s", texto);
            send(client_fd, respuesta, strlen(respuesta), 0);
        } else {
            char *rechazo = "Rechazado";  // Mensaje de rechazo
            send(client_fd, rechazo, strlen(rechazo), 0);
        }

        close(client_fd);  // Cierra conexión con el cliente
    }

    close(server_fd);  // Cierra socket del servidor
    return 0;
}