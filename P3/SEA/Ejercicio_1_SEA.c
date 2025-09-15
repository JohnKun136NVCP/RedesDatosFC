#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define TAM_BUFFER 1024

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <ARCHIVO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *servidor_ip = argv[1];
    int puerto_cliente = atoi(argv[2]);  // Puerto al que me conecto
    int desplazamiento = atoi(argv[3]);
    char *nombre_archivo = argv[4];
    int puerto_objetivo = 49200;  
    int cliente_fd;
    struct sockaddr_in direccion_servidor;
    char buffer[TAM_BUFFER] = {0};

    // Creamos el socket
    if ((cliente_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configuramos dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(puerto_cliente);
    if (inet_pton(AF_INET, servidor_ip, &direccion_servidor.sin_addr) <= 0) {
        perror("Dirección inválida");
        close(cliente_fd);
        exit(EXIT_FAILURE);
    }

    // Conectamos al servidor
    if (connect(cliente_fd, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("Conexión fallida");
        close(cliente_fd);
        exit(EXIT_FAILURE);
    }

    // Leemos archivo
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        perror("Error al abrir archivo");
        close(cliente_fd);
        exit(EXIT_FAILURE);
    }

    fseek(archivo, 0, SEEK_END);
    long tam_archivo = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);

    char *contenido = malloc(tam_archivo + 1);
    fread(contenido, 1, tam_archivo, archivo);
    contenido[tam_archivo] = '\0';
    fclose(archivo);

    // ENVIAR SIEMPRE puerto 49200 
    sprintf(buffer, "%d %d %s", puerto_objetivo, desplazamiento, contenido);
    send(cliente_fd, buffer, strlen(buffer), 0);

    printf("Enviando a servidor %d: Puerto objetivo=%d, Desplazamiento=%d\n", 
           puerto_cliente, puerto_objetivo, desplazamiento);

    // Recibimos respuesta del servidor
    int bytes_recibidos = recv(cliente_fd, buffer, TAM_BUFFER - 1, 0);
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0';
        printf("Respuesta del servidor (puerto %d): %s\n", puerto_cliente, buffer);
    } else {
        printf("No se recibió respuesta del servidor (puerto %d)\n", puerto_cliente);
    }

    free(contenido);
    close(cliente_fd);
    return 0;
}