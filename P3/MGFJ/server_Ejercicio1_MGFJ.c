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

#define BUFFER_SIZE 1024

// Cifrado César
static void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c; // los caracteres no alfabéticos se quedan sin cambio
        }
    }
}

// Lee una línea desde el socket
static ssize_t recv_line(int fd, char *line, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        char c; ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) { if (i == 0) return 0; break; } // conexión cerrada
        if (n < 0) return -1; // error
        line[i++] = c;
        if (c == '\n') break; // fin de línea
    }
    line[i] = '\0';
    return (ssize_t)i;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); return 1; }
    int PORT = atoi(argv[1]);
    if (PORT <= 0 || PORT > 65535) { fprintf(stderr, "[-] Puerto invalido: %s\n", argv[1]); return 1; }

    // Crear socket servidor
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) { perror("[-] socket"); return 1; }

    // Reutilizar dirección/puerto
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // Configurar dirección del servidor
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Asociar socket
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind"); close(server_sock); return 1;
    }
    // Escuchar conexiones entrantes
    if (listen(server_sock, 8) < 0) { perror("[-] listen"); close(server_sock); return 1; }

    printf("[+] SERVER %d LISTENING...\n", PORT);

    // Aceptar conexión cliente
    struct sockaddr_in client_addr; socklen_t addr_size = sizeof(client_addr);
    int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) { perror("[-] accept"); close(server_sock); return 1; }
    printf("[+] CLIENT CONNECTED TO %d\n", PORT);

    char line[4096], filename[512] = {0};
    int target_port = -1, k = 0;

    // Recibir cabecera: TARGET, SHIFT, NAME
    if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "TARGET %d", &target_port) != 1) {
        printf("[-] Bad TARGET\n"); goto done;
    }
    if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "SHIFT %d", &k) != 1) {
        printf("[-] Bad SHIFT\n"); goto done;
    }
    if (recv_line(client_sock, line, sizeof(line)) <= 0 || sscanf(line, "NAME %511[^\n]", filename) != 1) {
        printf("[-] Bad NAME\n"); goto done;
    }
    if (recv_line(client_sock, line, sizeof(line)) < 0) {
        printf("[-] Bad BLANK LINE\n"); goto done;
    }

    // Recibir archivo
    size_t cap = 8192, len = 0; char *content = (char *)malloc(cap);
    if (!content) { perror("[-] malloc"); goto done; }
    for (;;) {
        char buf[4096];
        ssize_t n = recv(client_sock, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) { perror("[-] recv"); free(content); content = NULL; break; }
        if (len + (size_t)n + 1 > cap) { // expandir buffer
            size_t newcap = cap * 2;
            char *tmp = realloc(content, newcap);
            if (!tmp) { free(content); content = NULL; break; }
            content = tmp; cap = newcap;
        }
        memcpy(content + len, buf, (size_t)n); len += (size_t)n;
    }
    if (!content) goto done;
    content[len] = '\0';

    // Verificar puerto destino
    if (target_port == PORT) {
        encryptCaesar(content, k); // cifrar contenido

        // Obtener nombre base sin extensión
        const char *base = filename;
        const char *slash = strrchr(filename, '/');
        if (slash && *(slash + 1) != '\0') base = slash + 1;

        char base_no_ext[512];
        const char *dot = strrchr(base, '.');
        size_t blen = dot ? (size_t)(dot - base) : strlen(base);
        if (blen >= sizeof(base_no_ext)) blen = sizeof(base_no_ext) - 1;
        memcpy(base_no_ext, base, blen);
        base_no_ext[blen] = '\0';

        // Nombre de archivo resultante
        char formatted_name[600];
        snprintf(formatted_name, sizeof(formatted_name), "%s_%d_cesar.txt", base_no_ext, PORT);

        // Enviar respuesta y archivo cifrado
        dprintf(client_sock, "OK %d\nNAME %s\n\n", PORT, formatted_name);
        send(client_sock, content, len, 0);

        printf("[+] SERVER %d: Request accepted.\n", PORT);
        printf("[+] SERVER %d: File received and encrypted.\n", PORT);
        if (len > 0) printf("\n%s\n", content); // mostrar contenido cifrado
    } else {
        dprintf(client_sock, "REJECT %d\n", PORT); // rechazar si el puerto no coincide
        printf("[-] SERVER %d: Request rejected (client requested port %d)\n", PORT, target_port);
    }
    free(content);

done:
    close(client_sock);
    close(server_sock);
    return 0;
}
