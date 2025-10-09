#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> //

#define PORT 49200
#define BUFFER_SIZE 8192 // Aumentado para archivos más grandes

int main(int argc, char *argv[]) {
    // Verificar que se pasó el alias como argumento
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <alias_servidor>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *server_alias = argv[1];

    int servidor_fd, cliente_sock;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    // Crear el socket del servidor
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd == -1) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor para enlazarla al alias específico
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Resolver el alias (ej: "s01") a su dirección IP usando /etc/hosts
    struct hostent *host_info = gethostbyname(server_alias);
    if (host_info == NULL) {
        fprintf(stderr, "Error: No se pudo resolver el host '%s'. Revisa tu /etc/hosts.\n", server_alias);
        exit(EXIT_FAILURE);
    }
    // Copiar la IP resuelta a la estructura de la dirección del servidor
    server_addr.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);
    
    // Enlazar (bind) el socket a la IP y puerto especificados
    if (bind(servidor_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind. Asegúrate de que el alias y la IP son correctos y no están en uso");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Poner el servidor en modo de escucha
    if (listen(servidor_fd, 5) < 0) {
        perror("Error en listen");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Bucle para aceptar conexiones de clientes
    while (1) {
        printf("[Servidor '%s'] escuchando en el puerto %d...\n", server_alias, PORT);

        addr_size = sizeof(struct sockaddr_in);
        cliente_sock = accept(servidor_fd, NULL, NULL);
        if (cliente_sock < 0) {
            perror("Error en accept");
            continue; // Intentar aceptar la siguiente conexión
        }
        printf("[Servidor] cliente conectado a %s.\n", server_alias);

        // Lógica para recibir el archivo
        char filename[512] = {0};
        char file_content[BUFFER_SIZE] = {0};

        // Recibir primero el nombre del archivo
        int bytes_recibidos = recv(cliente_sock, filename, sizeof(filename) - 1, 0);
        if (bytes_recibidos <= 0) {
            fprintf(stderr, "Error al recibir el nombre del archivo.\n");
            close(cliente_sock);
            continue;
        }
        filename[bytes_recibidos] = '\0'; // Asegurar terminación del string

        // Recibir después el contenido del archivo
        bytes_recibidos = recv(cliente_sock, file_content, sizeof(file_content) - 1, 0);
        if (bytes_recibidos <= 0) {
            fprintf(stderr, "Error al recibir el contenido del archivo.\n");
            close(cliente_sock);
            continue;
        }
        file_content[bytes_recibidos] = '\0';

        // Lógica para guardar el archivo en el directorio correcto
        char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            fprintf(stderr, "No se pudo obtener el directorio HOME.\n");
            close(cliente_sock);
            continue;
        }

        char ruta_completa[1024];
        // Construir la ruta final: /home/tu_usuario/s01/nombre_del_archivo.txt
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s/%s", home_dir, server_alias, filename);

        printf("Intentando guardar el archivo en: %s\n", ruta_completa);
        
        FILE *fp = fopen(ruta_completa, "w");
        if (fp == NULL) {
            perror("Error al abrir el archivo para escritura");
        } else {
            fputs(file_content, fp);
            fclose(fp);
            printf("¡Archivo guardado exitosamente en el directorio '%s'!\n", server_alias);
            // Enviar confirmación al cliente
            send(cliente_sock, "OK: Archivo guardado", strlen("OK: Archivo guardado"), 0);
        }

        close(cliente_sock);
        printf("\n"); // Línea en blanco para separar logs de conexiones
    }

    close(servidor_fd);
    return 0;
}
