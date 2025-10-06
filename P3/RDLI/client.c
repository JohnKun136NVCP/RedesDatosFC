#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define BUFFER_SIZE 1024

void encryptCaesar(char *text, int shift) {
        shift = shift % 26;
        for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                // Si la letra es mayuscula la desplazamos sobre las mayusculas 
                if (isupper(c)) {
                        text[i] = ((c - 'A' + shift) % 26) + 'A';
                // si la letra es miniscula la desplazamos sobre las minusculas
                } else if (islower(c)) {
                        text[i] = ((c - 'a' + shift) % 26) + 'a';
                } else {
                        text[i] = c;
                }
        }
}

int main(int argc, char *argv[]) {
    // New arguments
    // ./client <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre del archivo>
    if (argc != 5) {
        printf("Type: %s <server_ip> <port> <shift> <filename>\n", argv[0]);
        return 1;
    }else{
        printf("[+] Starting client...\n");
        printf("[+] Server IP: %s\n", argv[1]);
        printf("[+] Port: %s\n", argv[2]);
        printf("[+] Shift: %s\n", argv[3]);
        printf("[+] Filename: %s\n", argv[4]);
    }

    char *server_ip = argv[1]; 
    int PORT = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *filename = argv[4];
    
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE] = {0};
    char mensaje_con_shift[BUFFER_SIZE * 2] = {0}; // Buffer más grande

    // Create socket
    client_sock = socket(AF_INET, SOCK_STREAM, 0);

    // Check created correctly 
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }else{
        printf("[+] Socket created successfully\n");
    }

    // Define the server address. Esto es parte de la estructura sockaddr_in
    serv_addr.sin_family = AF_INET; // Familia de direcciones IPv4
    serv_addr.sin_port = htons(PORT); // Puerto
    // Mas moderno 
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("[-] Invalid address");
        close(client_sock);
        return 1;
    }


    // Connect to the server
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        printf("errno: %d (%s)\n", errno, strerror(errno));
        close(client_sock);
        return 1;
    }

    // Connection successful
    printf("[+] Connected to server\n");


    // Cipher the message
    //Get the message to cipher from the file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Error to open the file");
        close(client_sock);
        return 1;
    }



    size_t bytes_read = fread(mensaje, sizeof(char), BUFFER_SIZE - 1, fp);
    mensaje[bytes_read] = '\0';  // Asegurar terminación nula
    fclose(fp);

    printf("[+] Original message: %s\n", mensaje);

    encryptCaesar(mensaje, shift);
    printf("[+] Encrypted message: %s\n", mensaje);

    snprintf(mensaje_con_shift, sizeof(mensaje_con_shift), "%s|%d", mensaje, shift);
    send(client_sock, mensaje_con_shift, strlen(mensaje_con_shift), 0);

    printf("[+][Client] Sent encrypted message with shift: %s\n", mensaje_con_shift);


    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+][Client] Server response: %s\n", buffer);
    } else {
        printf("[-] No response from server\n");
    }

    close(client_sock);
    return 0;
}