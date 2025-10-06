#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>

#define TAM_BUFFER 1024
#define PUERTO_BASE 49200

void cifrar_cesar(char *texto, int desplazamiento) {
    desplazamiento = desplazamiento % 26;
    for (int i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (isupper(c)) {
            texto[i] = ((c - 'A' + desplazamiento) % 26) + 'A';
        } else if (islower(c)) {
            texto[i] = ((c - 'a' + desplazamiento) % 26) + 'a';
        } else {
            texto[i] = c;
        }
    }
}

void guardar_log(const char *mensaje) {
    FILE *log = fopen("server_log.txt", "a");
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
    if (argc != 2) {
        printf("Uso: %s <NOMBRE_SERVIDOR>\n", argv[0]);
        printf("Ejemplo: %s s01\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *nombre_servidor = argv[1];
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;
    char buffer[TAM_BUFFER] = {0};
    int next_puerto = PUERTO_BASE + 1;

    printf("Iniciando servidor %s en puerto %d...\n", nombre_servidor, PUERTO_BASE);
    guardar_log("Servidor iniciado");

    // Creamos el socket principal
    if ((servidor_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configuramos dirección
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(PUERTO_BASE);

    // Enlazamos un socket
    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error al enlazar");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchamos las conexiones
    if (listen(servidor_fd, 5) < 0) {
        perror("Error al escuchar");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        socklen_t tam_direccion = sizeof(direccion);
        if ((nuevo_socket = accept(servidor_fd, (struct sockaddr *)&direccion, &tam_direccion)) < 0) {
            perror("Error al aceptar conexión");
            continue;
        }
        // See asigna el puerto
        int puerto_asignado = next_puerto++;
        send(nuevo_socket, &puerto_asignado, sizeof(puerto_asignado), 0);
        
        char log_msg[100];
        sprintf(log_msg, "Cliente conectado. Puerto asignado: %d", puerto_asignado);
        printf("%s\n", log_msg);
        guardar_log(log_msg);
        
        close(nuevo_socket);
    }

    close(servidor_fd);
    return 0;
}