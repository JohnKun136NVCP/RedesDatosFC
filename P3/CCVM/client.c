#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTS {49200, 49201, 49202}
#define BUFFER_SIZE 1024

char* read_file(const char *filename, char *buffer, size_t buffer_size) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] No se puede abrir el archivo");
        return NULL;
    }
    size_t read_size = fread(buffer, 1, buffer_size - 1, fp);
    buffer[read_size] = '\0';
    
    fclose(fp);
    return buffer;
}

void send_and_receive_message(int server_port, int actual_port, int client_sock, const char *filename, const char *shift) {
    char file_buffer[BUFFER_SIZE * 10];
    char recv_buffer[BUFFER_SIZE * 10];
    char message[BUFFER_SIZE * 10]; // Just in case
    char *file_content = read_file(filename, file_buffer, sizeof(file_buffer));
    snprintf(message, sizeof(message), "%s|%d|%s", file_content, actual_port, shift);
    
    send(client_sock, message, strlen(message), 0);
    
    int bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
    if (bytes > 0) {
        recv_buffer[bytes] = '\0';
        printf("[*] SERVER RESPONSE %d: %s\n", server_port, recv_buffer);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Tipo: %s <servidor_ip> <port> <desplazamiento> <nombre del archivo> \n", argv[0]);
        return 1;
    }
    
    int ports[] = PORTS;
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *archivo = argv[4];
    char *shift = argv[3];
    
    int client_sock1, client_sock2, client_sock3;
    struct sockaddr_in serv_addr;
    
    client_sock1 = socket(AF_INET, SOCK_STREAM, 0);
    client_sock2 = socket(AF_INET, SOCK_STREAM, 0);
    client_sock3 = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock1 == -1 || client_sock2 == -1 || client_sock3 == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ports[0]);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    if (connect(client_sock1, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock1);
    }
    send_and_receive_message(ports[0], port, client_sock1, archivo, shift);
    
    serv_addr.sin_port = htons(ports[1]);
    if (connect(client_sock2, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock2);
    }
    send_and_receive_message(ports[1], port, client_sock2, archivo, shift);
    
    serv_addr.sin_port = htons(ports[2]);
    if (connect(client_sock3, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock3);
    }
    send_and_receive_message(ports[2], port, client_sock3, archivo, shift);
    
    close(client_sock1);
    close(client_sock2);
    close(client_sock3);
    
    return 0;
}
