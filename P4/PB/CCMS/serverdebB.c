#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h> //Manejo de tiempo para timestamps
#include <sys/stat.h> //Para hacer directorio
#include <sys/types.h>

#define PORT 49200 //Puerto de enlace al servidor. 

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

//Servidor de sockets
//Recibimos el úerto a que nos vamos a conectar y tambien la ip.
int create_server_socket(char *ip , int port) {
    //descriptor de socket
    int sockfd;
    struct sockaddr_in server_addr;
    
    //TCP sockets
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[-] Error creando socket");
        exit(1);
    }
    
    //Limpieza del buffer.
    memset(&server_addr, 0, sizeof(server_addr));
    //Configuracion inicial de familia, port y ips.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    //Cambio del ANY para reslover los alias.
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("[-] Error en inet_pton");
        exit(1);
    }
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        exit(1);
    }

    if (listen(sockfd, 5) < 0) {
        perror("[-] Error en listen");
        exit(1);
    }

    printf("[+] Escuchando en puerto %d...\n", port);
    return sockfd;
}


//Funcion main.
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <alias/IP>\n", argv[0]);
        exit(1);
    }
    //Declaracion de socket con alias.
    char *alias = argv[1];
    int sock = create_server_socket(alias, PORT);

    struct sockaddr_in client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    int client_sock;

    while (1) {
        printf("[+] Esperando conexión en %s:%d...\n", alias, PORT);
        addr_size = sizeof(client_addr);
        client_sock = accept(sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] Error en accept");
            continue;
        }

        // obtener IP cliente
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        printf("[+] Conexión desde %s en servidor %s\n", client_ip_str, alias);

        int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';

            int shift;
            char texto[BUFFER_SIZE];
            sscanf(buffer, "%d %[^\n]", &shift, texto);

            caesarCrypt(texto, shift);

            // Crear carpeta ~/s01, ~/s02, etc.
            char dirpath[256];
            char *home = getenv("HOME");
            if (!home) home = "/tmp";
            snprintf(dirpath, sizeof(dirpath), "%s/%s", home, alias);
            mkdir(dirpath, 0755);

            // archivo con timestamp
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char filename[512];
            snprintf(filename, sizeof(filename),
                     "%s/file_%04d%02d%02d_%02d%02d%02d.txt",
                     dirpath, t->tm_year+1900, t->tm_mon+1, t->tm_mday,
                     t->tm_hour, t->tm_min, t->tm_sec);

            FILE *fp = fopen(filename, "w");
            if (fp) {
                fputs(texto, fp);
                fclose(fp);
            }

            send(client_sock, "ARCHIVO CIFRADO RECIBIDO",
                 strlen("ARCHIVO CIFRADO RECIBIDO") + 1, 0);

            printf("[+] Archivo guardado como %s\n", filename);
        }
        close(client_sock);
    }

    close(sock);
    return 0;
}
