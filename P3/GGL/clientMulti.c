#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

void send_to_port(char *server_ip, int port, int shift, char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("[-] Error al abrir archivo");
        return;
    }

    char content[BUFFER_SIZE] = {0};
    fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(server_ip)
    };

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error de conexión");
        close(sock);
        return;
    }

    char message[BUFFER_SIZE * 2];
    snprintf(message, sizeof(message), "%d %d %s", port, shift, content);
    send(sock, message, strlen(message), 0);

    char response[BUFFER_SIZE];
    int bytes = recv(sock, response, sizeof(response) - 1, 0);
    if (bytes > 0) {
        response[bytes] = '\0';
        printf("[+] Respuesta puerto %d: %s\n", port, response);
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Uso: %s <IP> <puerto1> <puerto2> <puerto3> <archivo1> <archivo2> <archivo3> <desplazamiento>\n", argv[0]);
        printf("Ejemplo: %s 192.168.0.100 49200 49201 49202 mensaje1.txt mensaje2.txt mensaje3.txt 49\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int shift = atoi(argv[argc - 1]);

    printf("[+] IP del servidor: %s\n", ip);
    printf("[+] Desplazamiento: %d\n", shift);

    // Procesar puertos (argumentos 2, 3, 4)
    int ports[3];
    ports[0] = atoi(argv[2]);
    ports[1] = atoi(argv[3]);
    ports[2] = atoi(argv[4]);

    // Procesar archivos (argumentos 5, 6, 7)
    char *files[3];
    files[0] = argv[5];
    files[1] = argv[6];
    files[2] = argv[7];

    // Enviar cada archivo a su puerto correspondiente
    for (int i = 0; i < 3; i++) {
        printf("\n[+] Enviando archivo '%s' al puerto %d\n", files[i], ports[i]);
        send_to_port(ip, ports[i], shift, files[i]);
    }

    return 0;
}