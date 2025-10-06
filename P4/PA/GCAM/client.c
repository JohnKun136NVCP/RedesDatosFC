#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 8192

// Función auxiliar para registrar eventos en log.txt
void write_log(const char *msg) {
    FILE *fp = fopen("log.txt", "a");
    if (!fp) {
        perror("No se pudo abrir log.txt");
        return;
    }
    time_t now = time(NULL);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(fp, "%s - %s\n", tbuf, msg);
    fclose(fp);
    printf("%s - %s\n", tbuf, msg);
}

// Enviar archivo completo
void send_file(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error abriendo archivo");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, n, 0) == -1) {
            perror("Error enviando archivo");
            break;
        }
    }

    fclose(fp);
}

// Manejo de conexión con servidor
void connect_and_send(const char *server_name, int port, const char *filename) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *he;

    // Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creando socket");
        return;
    }

    // Resolver host
    he = gethostbyname(server_name);
    if (!he) {
        perror("No se pudo resolver host");
        close(sockfd);
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr *)he->h_addr);

    // Conectar
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al conectar");
        close(sockfd);
        return;
    }

    char buffer[BUFFER_SIZE];
    int n;

    // 1. Recibir estado inicial
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        write_log(buffer);
    }

    // 2. Enviar nombre del archivo
    send(sockfd, filename, strlen(filename), 0);

    // 3. Confirmación del servidor
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        write_log(buffer);
    }

    // 4. Enviar archivo
    send_file(filename, sockfd);

    // 5. Mensaje de procesamiento
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        write_log(buffer);
    }

    // 6. Mensaje de guardado final
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        write_log(buffer);
    }

    write_log("Transmisión finalizada");

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ALIAS_SERVIDOR> <PUERTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    char *server_name = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];

    connect_and_send(server_name, port, filename);

    return 0;
}
