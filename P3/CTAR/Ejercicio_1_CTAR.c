#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//#define PORT 7006        // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//server.c

/*
    Función para encriptar texto usando el cifrado César
*/
void encryptCaesar(char *text, int shift) {
    // Ajustamos el desplazamiento y verificamos si las letras son mayúsculas o minúsculas
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

/*
    Función principal con la configuración del socket
*/
int main(int argc, char *argv[]){
    if (argc != 2) {
        printf("USE: %s <PORT>\n", argv[0]);
        exit(1);
    }

    int PORT = atoi(argv[1]);
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char file_content[BUFFER_SIZE] = {0};
    int shift;
    int requested_port;
    
    // Creamos el socket del servidor para la comunicación
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1){
        perror("[-] Error to create the socket");
        return 1;
    }
    
    int opt = 1;
    // Permitir la reutilización de la dirección después de cerrar el socket
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] Error on setsockopt");
        close(server_sock);
        return 1;
    }
    //Configuramos la dirección del servidor (IPv4, puerto, cualquier IP local).
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //Asignamos el socket a la dirección y puerto especificados
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    // Escuchamos conexiones entrantes
    if (listen(server_sock, 1) < 0){
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }
    printf("[*] LISTENING %d...\n", PORT);

    //Esperamos y aceptamos una conexión entrante
    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0){
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }

    //Recibimos <PORT>|<SHIFT>|<File>
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0){
            printf("[-] Missed file\n");
            close(client_sock);
            close(server_sock);
            return 1;
    }
    buffer[bytes] = '\0';

    // Extraemos el puerto solicitado, desplazamiento y contenido del archivo
    if (sscanf(buffer, "%d|%d|%[^\n]", &requested_port, &shift, file_content) != 3){
        char *msg = "Invalid format. Use: <PORT>|<SHIFT>|<CONTENT>\n";
        send(client_sock, msg, strlen(msg), 0);
        close(client_sock);
        close(server_sock);
        return 1;
    }

    // Verificamos si el puerto solicitado coincide con el del servidor
    if (requested_port == PORT && shift == 34){
        encryptCaesar(file_content, shift);
        char *response = "File received and encrypted";
        send(client_sock, response, strlen(response), 0);
        printf("[*] SERVER RESPONSE %d", PORT);
        printf("[*] SERVER RESPONSE %d File received and encrypted:\n%s\n", PORT, file_content);
    } else {
        char rejected_msg[BUFFER_SIZE];
        snprintf(rejected_msg, sizeof(rejected_msg), "REJECTED\n");
        send(client_sock, rejected_msg, strlen(rejected_msg), 0);
        printf("[SERVER %d] Request rejected (client requested port %d).\n", PORT, requested_port);
    }
    close(client_sock);
    close(server_sock);
    return 0;
}
