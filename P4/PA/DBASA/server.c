#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define BASE_PORT 49200

//Cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c))
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        else if (islower(c))
            text[i] = ((c - 'a' + shift) % 26) + 'a';
    }
}

//Logs
void send_log(int log_sock, const char *estado) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "[%s] %s\n", timestamp, estado);

    printf("%s", log_msg);

    if (log_sock > 0)
        send(log_sock, log_msg, strlen(log_msg), 0);
}

//Crear socket
int create_socket_on_port(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] Error creando socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("[-] IP inválida");
        close(sock);
        exit(1);
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    if (listen(sock, 1) < 0) {
        perror("[-] Error en listen");
        close(sock);
        exit(1);
    }

    return sock;
}

//Buscar puerto libre
int find_free_port(const char *ip) {
    int port = BASE_PORT + 1;
    while (port < 50000) {
        int test_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        if (bind(test_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            close(test_sock);
            return port;
        }
        close(test_sock);
        port++;
    }
    return -1;
}

//Obtener IP desde alias
char *resolve_alias_ip(const char *alias) {
    struct hostent *he = gethostbyname(alias);
    if (he == NULL) {
        herror("[-] No se pudo resolver el alias");
        exit(1);
    }

    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    return inet_ntoa(*addr_list[0]);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <alias_de_red>\n", argv[0]);
        return 1;
    }

    const char *alias = argv[1];
    const char *ip = resolve_alias_ip(alias);

    mkdir(alias, 0777);

    printf("[+] Alias: %s -> IP: %s\n", alias, ip);

    int main_sock, log_sock = -1, data_sock = -1;
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    main_sock = create_socket_on_port(ip, BASE_PORT);
    printf("[+] Servidor escuchando en %s:%d...\n", ip, BASE_PORT);
    send_log(log_sock, "en espera de conexión");

    while (1) {
        int client = accept(main_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client < 0) {
            perror("[-] Error en accept");
            continue;
        }

        //Primer cliente: conexión de control
        if (log_sock == -1) {
            int new_port = find_free_port(ip);
            if (new_port == -1) {
                printf("[-] No se encontró puerto libre\n");
                close(client);
                continue;
            }

            char msg[64];
            snprintf(msg, sizeof(msg), "ASIGNADO %d\n", new_port);
            send(client, msg, strlen(msg), 0);
            printf("[+] Cliente de control asignado al puerto %d\n", new_port);

            int control_sock = create_socket_on_port(ip, new_port);
            int control_client = accept(control_sock, (struct sockaddr *)&client_addr, &addr_size);
            if (control_client >= 0) {
                log_sock = control_client;
                send_log(log_sock, "en espera de conexión");
                printf("[+] Conexión de control establecida en %d\n", new_port);
            }
            close(control_sock);
            close(client);
        }

        //Segundo cliente: conexión de datos
        else if (data_sock == -1) {
            send_log(log_sock, "recibiendo información");
            printf("[+] Cliente de datos conectado\n");

            char buffer[BUFFER_SIZE];
            int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                perror("[-] Error al recibir mensaje");
                close(client);
                continue;
            }

            buffer[bytes] = '\0';
            int shift = 3;
            encryptCaesar(buffer, shift);

            printf("[+][Server] Mensaje recibido y encriptado:\n%s\n", buffer);

            // Guardar dentro de la carpeta del alias
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/mensaje_encriptado.txt", alias);

            FILE *out = fopen(filepath, "w");
            if (out) {
                fprintf(out, "%s\n", buffer);
                fclose(out);
                printf("[+] Mensaje encriptado guardado en %s\n", filepath);
            } else {
                perror("[-] No se pudo crear el archivo");
            }

            send_log(log_sock, "mandando información");
            send(client, "Archivo recibido y encriptado.\n", 31, 0);
            send_log(log_sock, "en espera de conexión");

            close(client);
            data_sock = -1;
        }

        else {
            send(client, "Servidor ocupado\n", 17, 0);
            close(client);
        }
    }

    close(main_sock);
    return 0;
}
