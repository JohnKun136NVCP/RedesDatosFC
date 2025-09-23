#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <host> <puerto> <archivo>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];

    struct hostent *he = gethostbyname(host);
    if (!he) { perror("gethostbyname"); return 1; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); close(sock); return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror("fopen"); close(sock); return 1; }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sock, buffer, n, 0) != (ssize_t)n) {
            perror("send"); fclose(fp); close(sock); return 1;
        }
    }

    fclose(fp);
    shutdown(sock, SHUT_WR);
    close(sock);

    printf("Archivo '%s' enviado a %s:%d\n", filename, host, port);
    return 0;
}
