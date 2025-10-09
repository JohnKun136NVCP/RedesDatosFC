#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_PORT 49200
#define BUFFER_SIZE 1024

// Obtiene la IP de un alias de red
char *get_ip_from_alias(const char *alias) {
    struct hostent *he = gethostbyname(alias);
    if (he == NULL) {
        perror("[-] Error obteniendo IP del alias");
        exit(1);
    }
    static char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, he->h_addr_list[0], ip, sizeof(ip));
    return ip;
}

// Conecta a una IP y puerto específicos
int connect_to(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] Error creando socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error conectando al servidor");
        close(sock);
        exit(1);
    }

    return sock;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias_red>\n", argv[0]);
        printf("Ejemplo: %s s01\n", argv[0]);
        exit(1);
    }

    const char *alias = argv[1];
    char *ip = get_ip_from_alias(alias);

    printf("[+] IP del alias %s: %s\n", alias, ip);

    int main_sock = connect_to(ip, BASE_PORT);
    printf("[+] Conectado al servidor base (%s:%d)\n", ip, BASE_PORT);

    send(main_sock, "ASSIGN\n", 7, 0);

    char response[BUFFER_SIZE];
    int bytes = recv(main_sock, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        perror("[-] No se recibió respuesta del servidor");
        close(main_sock);
        exit(1);
    }

    response[bytes] = '\0';
    printf("[+] Respuesta: %s", response);

    //parsear puerto asignado
    int new_port = 0;
    sscanf(response, "ASIGNADO %d", &new_port);
    if (new_port == 0) {
        fprintf(stderr, "[-] No se pudo obtener puerto asignado\n");
        close(main_sock);
        exit(1);
    }

    close(main_sock);
    printf("[+] Conectando al puerto de control %d...\n", new_port);

    int control_sock = connect_to(ip, new_port);
    printf("[+] Conexión de control establecida. Escuchando logs...\n\n");

    char filename[128];
    snprintf(filename, sizeof(filename), "logs_%s.txt", alias);

    FILE *log_file = fopen(filename, "a");
    if (!log_file) {
        perror("[-] No se pudo abrir archivo de logs");
        close(control_sock);
        exit(1);
    }

    printf("[+] Guardando logs en: %s\n\n", filename);

    while (1) {
        char log_line[BUFFER_SIZE];
        int n = recv(control_sock, log_line, sizeof(log_line) - 1, 0);
        if (n <= 0) {
            printf("[-] Conexión cerrada por el servidor.\n");
            break;
        }

        log_line[n] = '\0';

        printf("%s", log_line);
        fflush(stdout);

        fprintf(log_file, "%s", log_line);
        fflush(log_file);
    }

    fclose(log_file);
    close(control_sock);
    return 0;
}
