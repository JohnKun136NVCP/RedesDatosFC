#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//funcion para mandar la informacion del archivo de texto al servidor.
void send2Server(char *server_ip, int port, int shift, char *texto) {
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error al crear socket");
        return;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error al conectar");
        close(client_sock);
        return;
    }

    // Envia el formato al servidor con el puerto objetivo, el corrimiento y el texto a cifrar.
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%d %s", shift, texto);
    printf("Enviando a %d: %s\n", port, mensaje);
    send(client_sock, mensaje, strlen(mensaje), 0);

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Respuesta del servidor %d: %s\n", port, buffer);
    }

    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
         printf("Uso: %s <IP_SERVIDOR> <PUERTO1> <PUERTO2> ... <ARCHIVO1> <ARCHIVO2> ... <CORRIMIENTO>\n", argv[0]);
        return 1;
    }
    //El primer argumento siempre es ip del servidor.
    char *server_ip = argv[1];
    // Calculamos cuantos puertos y cuantos archivos hay
    // Supongamos en este caso que el numero de puertos = número de archivos
    int total_args = argc - 2; // Quitamos <prog> y <IP> para la cuenta
    int shift = atoi(argv[argc - 1]); // Ultimo argumento siempre es el corrimiento
    int half = total_args - 1; // Quitamos corrimiento

    // Mitad de puertos y mitad de archivos
    int count = half / 2;

    if (half % 2 != 0) {
        fprintf(stderr, "[-] Error: el número de puertos y archivos debe ser igual.\n");
        return 1;
    }

    // Recorrer puertos y archivos emparejados
    for (int i = 0; i < count; i++) {
        int port = atoi(argv[2 + i]); // Puertos empiezan en argv[2]
        char *archivo = argv[2 + count + i]; // Archivos empiezan después de los puertos

        // Leer archivo
        FILE *fp = fopen(archivo, "r");
        if (!fp) {
            perror("[-] No se pudo abrir el archivo");
            continue;
        }
        char texto[BUFFER_SIZE];
        size_t bytes = fread(texto, 1, sizeof(texto) - 1, fp);
        fclose(fp);
        texto[bytes] = '\0';

        // Enviar cada archivo a su puerto correspondiente
        send2Server(server_ip, port, shift, texto);
    }

    return 0;
}