#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PUERTOS 10
#define BUFFER_SIZE 4096

// ======= Función para leer el contenido de un archivo =======
char* leer_archivo(const char* nombre_archivo) {
    FILE *fp = fopen(nombre_archivo, "r");
    if (!fp) {
        perror("[-]No se pudo abrir el archivo");
        return NULL;
    }

    // Obtenemos el tamaño del archivo
    fseek(fp, 0, SEEK_END);
    long tamano = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Reservamos memoria para guardarlo completo
    char* contenido = malloc(tamano + 1);
    if (!contenido) {
        fclose(fp);
        return NULL;
    }

    // Lo leemos y cerramos
    size_t len = fread(contenido, 1, tamano, fp);
    fclose(fp);
    contenido[len] = '\0';

    return contenido;
}

// ======= Un archivo para un puerto específico =======
void procesar_archivo_con_servidores(const char* ip_servidor, int puerto_objetivo, int desplazamiento, const char* nombre_archivo) {
    printf("\n[*]Procesando archivo: %s para el puerto objetivo: %d\n", nombre_archivo, puerto_objetivo);

    char* texto = leer_archivo(nombre_archivo);
    if (!texto) {
        printf("[-]Error al leer el archivo %s. Saltando...\n", nombre_archivo);
        return;
    }

    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Creamos el socket del cliente
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        free(texto);
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto_objetivo);
    serv_addr.sin_addr.s_addr = inet_addr(ip_servidor);

    // Conexión al servidor
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[-]CLIENTE no pudo conectar al servidor en puerto %d\n", puerto_objetivo);
        close(sockfd);
        free(texto);
        return;
    }

    // Armamos el mensaje: puerto, desplazamiento y texto
    int tamano_mensaje = snprintf(NULL, 0, "%d %d %s", puerto_objetivo, desplazamiento, texto) + 1;
    char* mensaje = malloc(tamano_mensaje);
    if (!mensaje) {
        printf("[-]Error al asignar memoria para el mensaje\n");
        close(sockfd);
        free(texto);
        return;
    }
    snprintf(mensaje, tamano_mensaje, "%d %d %s", puerto_objetivo, desplazamiento, texto);
    
    // Enviamos y esperamos respuesta
    send(sockfd, mensaje, strlen(mensaje), 0);

    memset(buffer, 0, sizeof(buffer));
    int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[*]Respuesta del servidor (puerto %d): %s\n", puerto_objetivo, buffer);
    } else {
        printf("[-]CLIENTE: sin respuesta del servidor en puerto %d\n", puerto_objetivo);
    }

    free(mensaje);
    close(sockfd);
    free(texto);
}

// ======= Varios archivos contra varios puertos =======
void procesar_todos_a_todos(char* ip_servidor, int* puertos_objetivo, int num_puertos, int desplazamiento, int inicio_archivos, int argc, char* argv[]) {
    int puertos_servidor[3] = {49200, 49201, 49202};

    // Recorremos puertos solicitados y archivos
    for (int p = 0; p < num_puertos; p++) {
        for (int a = inicio_archivos; a < argc; a++) {
            char* texto = leer_archivo(argv[a]);
            if (!texto) {
                printf("[-]Error al leer el archivo %s. Saltando...\n", argv[a]);
                continue;
            }

            // Mandamos el mismo archivo a los 3 servidores definidos
            for (int i = 0; i < 3; i++) {
                int puerto_actual = puertos_servidor[i];
                int sockfd;
                struct sockaddr_in serv_addr;
                char buffer[BUFFER_SIZE];

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0) {
                    perror("socket");
                    continue;
                }

                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(puerto_actual);
                serv_addr.sin_addr.s_addr = inet_addr(ip_servidor);

                if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    printf("[-]CLIENTE no pudo conectar al servidor en puerto %d\n", puerto_actual);
                    close(sockfd);
                    continue;
                }

                int tamano_mensaje = snprintf(NULL, 0, "%d %d %s", puertos_objetivo[p], desplazamiento, texto) + 1;
                char* mensaje = malloc(tamano_mensaje);
                if (!mensaje) {
                    printf("[-]Error al asignar memoria para el mensaje\n");
                    close(sockfd);
                    continue;
                }
                snprintf(mensaje, tamano_mensaje, "%d %d %s", puertos_objetivo[p], desplazamiento, texto);

                send(sockfd, mensaje, strlen(mensaje), 0);

                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    printf("[*]Respuesta del servidor (puerto %d): %s\n", puerto_actual, buffer);
                } else {
                    printf("[-]CLIENTE: sin respuesta del servidor en puerto %d\n", puerto_actual);
                }

                free(mensaje);
                close(sockfd);
            }
            free(texto);
        }
    }
}

// ======= Cliente principal =======
int main(int argc, char *argv[]) {
    // Validamos argumentos
    if (argc < 5) {
        fprintf(stderr, "USO: %s <IP_SERVIDOR> <PUERTO1> [PUERTO2...] <DESPLAZAMIENTO> <archivo1.txt> [archivo2.txt ...]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 192.168.0.193 49200 49201 40 file1.txt file2.txt\n", argv[0]);
        return 1;
    }

    char *ip_servidor = argv[1];
    int desplazamiento;
    int puertos_objetivo[MAX_PUERTOS];
    int num_puertos = 0;
    int inicio_archivos = 0;

    // Interpretamos argumentos: primero los puertos, luego el desplazamiento y después los archivos
    for (int i = 2; i < argc; i++) {
        char* endptr;
        long val = strtol(argv[i], &endptr, 10);

        if (*endptr != '\0' || val < 1024 || val > 65535) {
            desplazamiento = atoi(argv[i]);
            if (desplazamiento == 0 && strcmp(argv[i], "0") != 0) {
                 fprintf(stderr, "Error: El argumento '%s' no es un puerto válido ni un desplazamiento.\n", argv[i]);
                 return 1;
            }
            inicio_archivos = i + 1;
            break;
        }

        puertos_objetivo[num_puertos++] = (int)val;
        if (num_puertos >= MAX_PUERTOS) {
            fprintf(stderr, "Demasiados puertos, el máximo es %d.\n", MAX_PUERTOS);
            return 1;
        }
    }

    int num_archivos = argc - inicio_archivos;

    if (num_puertos <= 0 || num_archivos <= 0 || inicio_archivos >= argc) {
        fprintf(stderr, "Error: Argumentos insuficientes o mal formados.\n");
        return 1;
    }

    printf("[*]Servidor: %s\n", ip_servidor);
    printf("[*]Desplazamiento: %d\n", desplazamiento);

    printf("[*]Puertos objetivo: ");
    for (int i = 0; i < num_puertos; i++) {
        printf("%d ", puertos_objetivo[i]);
    }
    printf("\n");

    printf("[*]Archivos a procesar: ");
    for (int i = 0; i < num_archivos; i++) {
        printf("%s ", argv[inicio_archivos + i]);
    }
    printf("\n");

    if (num_puertos == num_archivos) {
        // Caso 1: un archivo por puerto
        for (int i = 0; i < num_puertos; i++) {
            procesar_archivo_con_servidores(ip_servidor, puertos_objetivo[i], desplazamiento, argv[inicio_archivos + i]);
        }
    } else {
        // Caso 2: todos los archivos se envían a todos los puertos
        procesar_todos_a_todos(ip_servidor, puertos_objetivo, num_puertos, desplazamiento, inicio_archivos, argc, argv);
    }

    return 0;
}
