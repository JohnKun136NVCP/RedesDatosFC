#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//Funcion para obtener la informacion del sistema.

//Funcion de encriptado cesar.
void caesarCrypt(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}


//Funcion main.
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }
    //instrucciones del puerto a escuchar
    int PORT = atoi(argv[1]);
    //sockets
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Server listening port %d...\n", PORT);

    //espera la conexion de 1 o mas clientes.
    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] Error en accept");
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                close(client_sock);
                continue;
            }
        buffer[bytes] = '\0';
        // Recibe el formato indicado
        int puertoObjetivo, shift;
        char texto[BUFFER_SIZE];
        sscanf(buffer, "%d %d %[^\t\n]", &puertoObjetivo, &shift, texto);

        if (puertoObjetivo == PORT) {
            caesarCrypt(texto, shift);
            char respuesta[BUFFER_SIZE];
            snprintf(respuesta, sizeof(respuesta), "Procesado: %s", texto);
            printf("Recibido: puertoObjetivo=%d shift=%d texto='%s'\n", puertoObjetivo, shift, texto);
            send(client_sock, respuesta, strlen(respuesta), 0);
        } else {
            char *rechazo = "Rechazado";
            send(client_sock, rechazo, strlen(rechazo), 0);
        }
        close(client_sock);
    }
    close(server_sock);
    return 0;
}