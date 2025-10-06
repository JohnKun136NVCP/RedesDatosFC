#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define TAM_BUFFER 1024
#define PUERTO_BASE 49200

void guardar_log(const char *mensaje) {
    FILE *log = fopen("client_log.txt", "a");
    if (log) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, mensaje);
        fclose(log);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <SERVIDOR_IP> <DESPLAZAMIENTO> <ARCHIVO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *servidor_ip = argv[1];
    int desplazamiento = atoi(argv[2]);
    char *nombre_archivo = argv[3];
    int puerto_asignado;

    // PASO 1: Conectar al puerto (49200) 
    int socket_base = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in direccion_servidor;
    
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO_BASE);
    if (inet_pton(AF_INET, servidor_ip, &direccion_servidor.sin_addr) <= 0) {
        perror("Dirección inválida");
        close(socket_base);
        exit(EXIT_FAILURE);
    }

    // Conectamos al puerto base
    if (connect(socket_base, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("Conexión fallida al puerto base");
        close(socket_base);
        exit(EXIT_FAILURE);
    }

    // Recibimos puerto asignaddo del servidor
    if (recv(socket_base, &puerto_asignado, sizeof(puerto_asignado), 0) <= 0) {
        perror("Error al recibir puerto asignado");
        close(socket_base);
        exit(EXIT_FAILURE);
    }
    close(socket_base);

    char log_msg[100];
    sprintf(log_msg, "Puerto asignado recibido: %d", puerto_asignado);
    printf("%s\n", log_msg);
    guardar_log(log_msg);

    // Se conecta para enviar el archivo
    int socket_final = socket(AF_INET, SOCK_STREAM, 0);
    direccion_servidor.sin_port = htons(puerto_asignado);

    if (connect(socket_final, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("Conexión fallida al puerto asignado");
        close(socket_final);
        exit(EXIT_FAILURE);
    }

    // Leer archivo
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        perror("Error al abrir archivo");
        close(socket_final);
        exit(EXIT_FAILURE);
    }

    fseek(archivo, 0, SEEK_END);
    long tam_archivo = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);

    char *contenido = malloc(tam_archivo + 1);
    fread(contenido, 1, tam_archivo, archivo);
    contenido[tam_archivo] = '\0';
    fclose(archivo);

    // Enviar datos
    char buffer[TAM_BUFFER];
    sprintf(buffer, "%d %s", desplazamiento, contenido);
    send(socket_final, buffer, strlen(buffer), 0);

    sprintf(log_msg, "Archivo enviado al puerto %d: %s", puerto_asignado, nombre_archivo);
    printf("%s\n", log_msg);
    guardar_log(log_msg);

    // Recibir respuesta del servidor
    char respuesta[TAM_BUFFER];
    int bytes_recibidos = recv(socket_final, respuesta, TAM_BUFFER - 1, 0);
    if (bytes_recibidos > 0) {
        respuesta[bytes_recibidos] = '\0';
        printf("Respuesta del servidor: %s\n", respuesta);
        sprintf(log_msg, "Estado: %s", respuesta);
        guardar_log(log_msg);
    } else {
        printf("No se recibió respuesta del servidor\n");
        guardar_log("Estado: Sin respuesta");
    }

    free(contenido);
    close(socket_final);
    return 0;
}