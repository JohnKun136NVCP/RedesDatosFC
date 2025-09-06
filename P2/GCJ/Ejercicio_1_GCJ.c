#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define PORT 7006
#define BUFFER_SIZE 1024

// Nos permite realizar el cifrado César
void encryptCesar(char *text, int shift){
    shift = shift % 26;
    for (int i = 0; text[i] !='\0'; i++) {
        char c = text[i];
	if (isupper(c)){
           text[i] = ((c - 'A' + shift) % 26) + 'A'; 
        } else if(islower(c)) {
           text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main() {
    char clave[20];
    char shift_char[20];
    // Solo se leen palabras de menos de 20 caracteres.
    printf("Introduzca la palabra que desea cifrar: \n");
    fgets(clave, 20, stdin);
    // Eliminamos espacios extra.
    clave[strcspn(clave, "\n")] = 0;
    printf("Introduzca el shift que desea que tenga su palabra: \n"); 
    fgets(shift_char, 20, stdin);
    // Convertimos a los caracteres en enteros.
    int shift = atoi(shift_char);
    // Se hace la encripción
    encryptCesar(clave, shift);
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    char *server_ip = "192.168.201.101";
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip); 
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }
    printf("[+] Connected to server\n");
    snprintf(mensaje, sizeof(mensaje), "%s %s", clave, shift_char);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Key and shift was sent: %s\n", mensaje);
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+][Client] Server message: %s\n", buffer);
        if (strstr(buffer, "ACCESS GRANTED") != NULL) {
            FILE *fp = fopen("info.txt", "w");
            if (fp == NULL) {
                perror("[-] Error to open the file");
                close(client_sock);
                return 1;
            }
            while ((bytes = recv(client_sock, buffer, sizeof(buffer),0)) > 0) {
                fwrite(buffer, 1, bytes, fp);
            }
            printf("[+][Client] The file was save like 'info.txt'\n");
            fclose(fp);
        } 
    } else {
        printf("[-] Server connection tiemeout\n");
    }
    close(client_sock);
    return 0;
}

