#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#define BUFFER_SIZE 8192

// ======= Mapeo de alias a IPs =======
typedef struct {
    char *alias;
    char *ip;
} alias_ip_t;

alias_ip_t mapeo[] = {
    {"s01", "192.168.100.16"},
    {"s02", "192.168.100.18"},
    {NULL, NULL}  // Terminador
};

// ======= Función para obtener IP desde alias =======
char* obtener_ip(const char *alias) {
    for (int i = 0; mapeo[i].alias != NULL; i++) {
        if (strcmp(mapeo[i].alias, alias) == 0) {
            return mapeo[i].ip;
        }
    }
    return NULL; // No encontrado
}

// ======= Función de Cifrado César de práctica 2 =======
void encryptCesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;

    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];

        if (isupper(c)) {
            // Mayúsculas
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            // Minúsculas
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            // Caracteres no alfabéticos quedan igual
            text[i] = (char)c;
        }
    }
}

// ======= Servidor TCP =======
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USO: %s <ALIAS>\n", argv[0]);
        fprintf(stderr, "Aliases disponibles: s01, s02\n");
        return 1;
    }

    char *alias = argv[1];
    char *ip_servidor = obtener_ip(alias);
    int puerto = 49200;  // Puerto fijo para todos los alias
    
    if (ip_servidor == NULL) {
        fprintf(stderr, "Alias inválido: %s\n", alias);
        fprintf(stderr, "Aliases disponibles: s01, s02\n");
        return 1;
    }

    int sockfd = -1, newsockfd = -1;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];

    // ======= Crear socket TCP =======
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    // ======= Reusar puerto si se reinicia rápido =======
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ======= Configuración del servidor =======
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_servidor);  // Bind a la IP del alias
    serv_addr.sin_port = htons(puerto);

    // ======= Bind =======
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // ======= Listen =======
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("[*]SERVIDOR %s escuchando en puerto %d (IP: %s)...\n", alias, puerto, ip_servidor);

    // ======= Loop principal para aceptar conexiones =======
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) { perror("accept"); continue; }

        // ======= Leer petición: "<PUERTO_OBJ> <SHIFT> <TEXTO>" =======
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n <= 0) { close(newsockfd); continue; }

        int puerto_obj = 0, shift = 0;
        char texto[BUFFER_SIZE];
        texto[0] = '\0';

        if (sscanf(buffer, "%d %d %[^\n]", &puerto_obj, &shift, texto) < 2) {
            const char *errorFormato = "Formato inválido (esperado: <PUERTO> <DESPLAZAMIENTO> <TEXTO>)\n";
            write(newsockfd, errorFormato, strlen(errorFormato));
            close(newsockfd);
            continue;
        }

        if (puerto_obj != puerto) {
            // Rechazo
            printf("[*]SERVIDOR %s → Solicitud rechazada (cliente pidió puerto %d)\n",
                   alias, puerto_obj);
            char rechazo[128];
            snprintf(rechazo, sizeof(rechazo), "Rechazado por servidor %s en puerto %d\n", alias, puerto);
            write(newsockfd, rechazo, strlen(rechazo));
            close(newsockfd);
            continue;
        }

        // Aceptado
        printf("[*]SERVIDOR %s → Solicitud aceptada\n", alias);
        encryptCesar(texto, shift);
        printf("[*]SERVIDOR %s → Archivo recibido y cifrado: %s\n", alias, texto);

        // ======= Responder al cliente =======
        char respuesta[BUFFER_SIZE];
        // Limito el %s a 4000 caracteres para evitar warning de truncamiento
        snprintf(respuesta, sizeof(respuesta),
                 "Procesado por servidor %s en puerto %d: %.4000s\n", alias, puerto, texto);
        write(newsockfd, respuesta, strlen(respuesta));

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}

