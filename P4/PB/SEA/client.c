#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

#define TAM_BUFFER 1024
#define PUERTO_BASE 49200
#define TIMEOUT 10

void guardar_log(const char *tipo, const char *servidor, const char *mensaje) {
    char nombre_archivo[50];
    sprintf(nombre_archivo, "client_%s.log", tipo);
    
    FILE *log = fopen(nombre_archivo, "a");
    if (log) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s -> %s: %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, 
                tipo, servidor, mensaje);
        fclose(log);
    }
}

int enviar_archivo(const char *servidor_ip, const char *servidor_nombre, int desplazamiento, const char *nombre_archivo) {
    int socket_fd;
    struct sockaddr_in direccion_servidor;
    struct timeval timeout;
    
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    // Crear socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error al crear socket\n");
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: No se pudo crear socket");
        return -1;
    }

    // Configurar timeout
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Configurar dirección
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO_BASE);
    if (inet_pton(AF_INET, servidor_ip, &direccion_servidor.sin_addr) <= 0) {
        printf("Dirección inválida: %s\n", servidor_ip);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Dirección inválida");
        close(socket_fd);
        return -1;
    }

    // Conectar al servidor
    if (connect(socket_fd, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        printf("Error al conectar con %s (%s)\n", servidor_nombre, servidor_ip);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Conexión fallida");
        close(socket_fd);
        return -1;
    }

    // Leer archivo
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        printf("Error al abrir archivo: %s\n", nombre_archivo);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Archivo no encontrado");
        close(socket_fd);
        return -1;
    }

    fseek(archivo, 0, SEEK_END);
    long tam_archivo = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);

    if (tam_archivo >= TAM_BUFFER - 50) {
        printf("Archivo demasiado grande: %s\n", nombre_archivo);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Archivo demasiado grande");
        fclose(archivo);
        close(socket_fd);
        return -1;
    }

    char *contenido = malloc(tam_archivo + 1);
    fread(contenido, 1, tam_archivo, archivo);
    contenido[tam_archivo] = '\0';
    fclose(archivo);

    // Enviar datos (desplazamiento + contenido)
    char buffer[TAM_BUFFER];
    sprintf(buffer, "%d %s", desplazamiento, contenido);
    
    if (send(socket_fd, buffer, strlen(buffer), 0) < 0) {
        printf("Error al enviar datos a %s\n", servidor_nombre);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Fallo en envío");
        free(contenido);
        close(socket_fd);
        return -1;
    }

    // Recibir respuesta del servidor
    char respuesta[TAM_BUFFER];
    int bytes_recibidos = recv(socket_fd, respuesta, TAM_BUFFER - 1, 0);
    
    if (bytes_recibidos > 0) {
        respuesta[bytes_recibidos] = '\0';
        printf("%s: %s\n", servidor_nombre, respuesta);
        
        char log_msg[150];
        sprintf(log_msg, "Archivo: %s, Respuesta: %s", nombre_archivo, respuesta);
        guardar_log("ARCHIVO", servidor_nombre, log_msg);
    } else {
        printf("%s: No se recibió respuesta\n", servidor_nombre);
        guardar_log("ARCHIVO", servidor_nombre, "ERROR: Sin respuesta");
    }

    free(contenido);
    close(socket_fd);
    return 0;
}

int verificar_status(const char *servidor_ip, const char *servidor_nombre) {
    int socket_fd;
    struct sockaddr_in direccion_servidor;
    struct timeval timeout;
    
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        guardar_log("STATUS", servidor_nombre, "ERROR: Socket fallido");
        return -1;
    }

    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO_BASE);
    inet_pton(AF_INET, servidor_ip, &direccion_servidor.sin_addr);

    if (connect(socket_fd, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        printf("%s: DESCONECTADO\n", servidor_nombre);
        guardar_log("STATUS", servidor_nombre, "DESCONECTADO");
        close(socket_fd);
        return 0;
    }

    // Enviar mensaje de status
    char mensaje[100];
    sprintf(mensaje, "STATUS_CHECK from client");
    send(socket_fd, mensaje, strlen(mensaje), 0);

    // Recibir respuesta
    char buffer[TAM_BUFFER];
    int bytes_recibidos = recv(socket_fd, buffer, TAM_BUFFER - 1, 0);
    
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0';
        printf("%s: %s\n", servidor_nombre, buffer);
        guardar_log("STATUS", servidor_nombre, "CONECTADO");
    } else {
        printf("%s: Sin respuesta\n", servidor_nombre);
        guardar_log("STATUS", servidor_nombre, "SIN_RESPUESTA");
    }

    close(socket_fd);
    return 1;
}

void mostrar_uso() {
    printf("Uso:\n");
    printf("  Enviar archivo:    %s <servidor> <desplazamiento> <archivo>\n", "client");
    printf("  Verificar status:  %s status <servidor>\n", "client");
    printf("  Verificar todos:   %s status-all\n", "client");
    printf("  Envío aleatorio:   %s random <archivo>\n", "client");
    printf("\nServidores disponibles: s01, s02, s03, s04\n");
    printf("Ejemplos:\n");
    printf("  %s s01 15 archivo.txt\n", "client");
    printf("  %s status s02\n", "client");
    printf("  %s status-all\n", "client");
    printf("  %s random archivo.txt\n", "client");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        mostrar_uso();
        exit(EXIT_FAILURE);
    }

    // Mapeo de servidores a IPs
    struct Servidor {
        char *nombre;
        char *ip;
    };
    
    struct Servidor servidores[] = {
        {"s01", "192.168.0.101"},
        {"s02", "192.168.0.102"},
        {"s03", "192.168.0.103"},
        {"s04", "192.168.0.104"},
        {NULL, NULL}
    };

    if (strcmp(argv[1], "status") == 0 && argc == 3) {
        // Verificar status individual
        char *servidor_buscado = argv[2];
        char *ip_encontrada = NULL;
        
        for (int i = 0; servidores[i].nombre != NULL; i++) {
            if (strcmp(servidores[i].nombre, servidor_buscado) == 0) {
                ip_encontrada = servidores[i].ip;
                break;
            }
        }
        
        if (ip_encontrada) {
            return verificar_status(ip_encontrada, servidor_buscado);
        } else {
            printf("Error: Servidor '%s' no encontrado\n", servidor_buscado);
            return -1;
        }
        
    } else if (strcmp(argv[1], "status-all") == 0) {
        // Verificar todos los servidores
        printf("=== VERIFICANDO ESTADO DE SERVIDORES ===\n");
        for (int i = 0; servidores[i].nombre != NULL; i++) {
            verificar_status(servidores[i].ip, servidores[i].nombre);
        }
        printf("=======================================\n");
        
    } else if (strcmp(argv[1], "random") == 0 && argc == 3) {
        // Envío aleatorio a servidor aleatorio
        srand(time(NULL));
        int servidor_idx = rand() % 4;
        int desplazamiento = rand() % 25 + 1;
        char *archivo = argv[2];
        
        printf("   Envío aleatorio:\n");
        printf("   Servidor: %s\n", servidores[servidor_idx].nombre);
        printf("   Desplazamiento: %d\n", desplazamiento);
        printf("   Archivo: %s\n", archivo);
        
        return enviar_archivo(servidores[servidor_idx].ip, 
                            servidores[servidor_idx].nombre, 
                            desplazamiento, archivo);
        
    } else if (argc == 4) {
        // Envío normal de archivo
        char *servidor_buscado = argv[1];
        int desplazamiento = atoi(argv[2]);
        char *archivo = argv[3];
        char *ip_encontrada = NULL;
        
        // Buscar IP del servidor
        for (int i = 0; servidores[i].nombre != NULL; i++) {
            if (strcmp(servidores[i].nombre, servidor_buscado) == 0) {
                ip_encontrada = servidores[i].ip;
                break;
            }
        }
        
        if (ip_encontrada) {
            return enviar_archivo(ip_encontrada, servidor_buscado, desplazamiento, archivo);
        } else {
            
            return enviar_archivo(servidor_buscado, servidor_buscado, desplazamiento, archivo);
        }
        
    } else {
        printf("Error: Parámetros incorrectos\n\n");
        mostrar_uso();
        return -1;
    }

    return 0;
}