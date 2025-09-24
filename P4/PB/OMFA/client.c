#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>       // <-- Incluido para gethostbyname
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <server_alias> <port> <file_to_send>\n", argv[0]);
        return 1;
    }

    char *server_alias = argv[1];
    int port = atoi(argv[2]);
    char *file_to_send = argv[3];

    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[-] Socket creation failed");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Resolver alias a IP
    if (inet_pton(AF_INET, server_alias, &serv_addr.sin_addr) <= 0) {
        // Intentar como nombre de host (s01, s02…)
        struct hostent *he;
        he = gethostbyname(server_alias);
        if (!he) {
            perror("[-] Host not found");
            close(sockfd);
            return 1;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Connection failed");
        close(sockfd);
        return 1;
    }

    printf("[+] Connected to server %s:%d\n", server_alias, port);

    // Enviar alias al servidor para que sepa dónde guardar
    send(sockfd, server_alias, strlen(server_alias), 0);

    // Abrir archivo
    FILE *fp = fopen(file_to_send, "rb");
    if (!fp) {
        perror("[-] Cannot open file");
        close(sockfd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) < 0) {
            perror("[-] Send failed");
            break;
        }
    }

    fclose(fp);
    printf("[+] File sent successfully\n");

    close(sockfd);
    return 0;
}
