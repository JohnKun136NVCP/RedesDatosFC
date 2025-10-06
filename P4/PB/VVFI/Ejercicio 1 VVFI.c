// Server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define BUFFER_SIZE 2048  

// Cifrado César
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <alias1> [alias2 ... aliasN] <puerto>\n", argv[0]);
        return 1;
    }

    int puerto_escucha = atoi(argv[argc - 1]);  // último argumento es el puerto
    int num_aliases = argc - 2;                 // alias = todos menos programa y puerto

    // Crear directorios para cada alias
    for (int i = 1; i <= num_aliases; i++) {
        if (mkdir(argv[i], 0777) < 0 && errno != EEXIST) {
            perror("[-] Error al crear directorio");
        }
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[-] Error al crear socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configuración de la dirección
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto_escucha);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("[-] Error en listen");
        close(server_fd);
        exit(1);
    }

    printf("[+] Servidor escuchando en puerto %d para alias:", puerto_escucha);
    for (int i = 1; i <= num_aliases; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[-] Error en accept");
            continue;
        }

        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("[-] Error al recibir");
            close(client_fd);
            continue;
        }
        buffer[bytes_received] = '\0';

        // Formato esperado: "<shift> <texto>"
        int shift;
        char texto[BUFFER_SIZE];
        if (sscanf(buffer, "%d %[^\t\n]", &shift, texto) < 2) {
            fprintf(stderr, "[-] Mensaje recibido con formato incorrecto\n");
            close(client_fd);
            continue;
        }

        // Aplicar cifrado
        encryptCaesar(texto, shift);

        // Guardar en resultado.txt en todos los alias
        for (int i = 1; i <= num_aliases; i++) {
            char ruta_archivo[BUFFER_SIZE];
            snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/resultado.txt", argv[i]);
            FILE *fp = fopen(ruta_archivo, "a");
            if (fp) {
                fprintf(fp, "%s\n", texto);
                fclose(fp);
            } else {
                perror("[-] Error al abrir archivo de salida");
            }
        }

        // Responder al cliente
        char respuesta[BUFFER_SIZE];
        snprintf(respuesta, sizeof(respuesta), "Procesado: %s", texto);
        send(client_fd, respuesta, strlen(respuesta), 0);

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
