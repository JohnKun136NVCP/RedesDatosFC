#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#define MAIN_PORT 49200
#define BUFFER_SIZE 1024

void send_message(const char *sender, const char *receiver) {
    struct addrinfo hints, *res;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", MAIN_PORT);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Obtener dirección IP del alias
    if (getaddrinfo(receiver, port_str, &hints, &res) != 0) {
        perror("[-] Error al resolver alias");
        return;
    }

    // Crear socket
    int client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_sock < 0) {
        perror("[-] Error al crear socket cliente");
        freeaddrinfo(res);
        return;
    }

    // Conectar al servidor
    if (connect(client_sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror ("[-] Error al conectar al servidor");
        close(client_sock);
        freeaddrinfo(res);
        return;
    }

    // Enviar mensaje
    char full_msg[BUFFER_SIZE];
    snprintf(full_msg, sizeof(full_msg), "%s -> %s : Hola soy %s",
             sender, receiver, sender);

    if(send(client_sock, full_msg, strlen(full_msg), 0) < 0){
        perror("[-] Error al enviar mensaje");
        close(client_sock);
        freeaddrinfo(res);
        return;
    }
    
    close(client_sock);
    freeaddrinfo(res);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias1> <alias2> ...\n", argv[0]);
        return 1;
    }

    int num_alias = argc - 1;
    char **alias_list = &argv[1];

    for (int i = 0; i < num_alias; i++) {
        for (int j = 0; j < num_alias; j++) {
            if (i == j) continue;
            send_message(alias_list[i], alias_list[j]);
            sleep(1); // evitar saturar
        }
    }

    return 0;
}
