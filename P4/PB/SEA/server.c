#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#include <sys/select.h>

#define TAM_BUFFER 1024
#define PUERTO_BASE 49200
#define MAX_CLIENTES 10

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

void guardar_log(const char *nombre_servidor, const char *mensaje) {
    char nombre_archivo[50];
    sprintf(nombre_archivo, "server_%s.log", nombre_servidor);
    
    FILE *log = fopen(nombre_archivo, "a");
    if (log) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, mensaje);
        fclose(log);
    }
}

void manejar_status_check(int cliente_socket, const char *nombre_servidor) {
    char respuesta[100];
    sprintf(respuesta, "STATUS_OK from %s", nombre_servidor);
    send(cliente_socket, respuesta, strlen(respuesta), 0);
    
    char log_msg[100];
    sprintf(log_msg, "Status check respondido correctamente");
    guardar_log(nombre_servidor, log_msg);
    
    printf("Status check respondido para %s\n", nombre_servidor);
}

void manejar_archivo(int cliente_socket, const char *nombre_servidor, const char *buffer) {
    int desplazamiento;
    char contenido_archivo[TAM_BUFFER];
    
    // Extraer desplazamiento y contenido del mensaje
    if (sscanf(buffer, "%d %[^\n]", &desplazamiento, contenido_archivo) != 2) {
        printf("Error: Formato de mensaje inválido\n");
        send(cliente_socket, "ERROR_FORMATO", 13, 0);
        return;
    }
    
    // Aplicar cifrado César al contenido
    cifrar_cesar(contenido_archivo, desplazamiento);
    
    // Crear directorio del servidor si no existe
    char comando_mkdir[100];
    sprintf(comando_mkdir, "mkdir -p %s", nombre_servidor);
    system(comando_mkdir);
    
    // Generar nombre de archivo con timestamp
    char nombre_archivo[256];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    
    sprintf(nombre_archivo, "%s/archivo_%04d%02d%02d_%02d%02d%02d.txt", 
            nombre_servidor,
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    // Guardar archivo cifrado
    FILE *archivo = fopen(nombre_archivo, "w");
    if (archivo) {
        fprintf(archivo, "%s", contenido_archivo);
        fclose(archivo);
        
        char log_msg[200];
        sprintf(log_msg, "Archivo guardado: %s, Desplazamiento: %d", nombre_archivo, desplazamiento);
        printf("%s\n", log_msg);
        guardar_log(nombre_servidor, log_msg);
        
        send(cliente_socket, "ARCHIVO_RECIBIDO_CORRECTAMENTE", 29, 0);
    } else {
        printf("Error al guardar archivo: %s\n", nombre_archivo);
        send(cliente_socket, "ERROR_GUARDAR_ARCHIVO", 21, 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <NOMBRE_SERVIDOR>\n", argv[0]);
        printf("Ejemplo: %s s01\n", argv[0]);
        printf("Servidores disponibles: s01, s02, s03, s04\n");
        exit(EXIT_FAILURE);
    }

    char *nombre_servidor = argv[1];
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;
    char buffer[TAM_BUFFER] = {0};

    // Validar nombre del servidor
    int servidor_valido = 0;
    char *servidores_validos[] = {"s01", "s02", "s03", "s04", NULL};
    
    for (int i = 0; servidores_validos[i] != NULL; i++) {
        if (strcmp(nombre_servidor, servidores_validos[i]) == 0) {
            servidor_valido = 1;
            break;
        }
    }
    
    if (!servidor_valido) {
        printf("Error: Nombre de servidor inválido. Usar: s01, s02, s03, s04\n");
        exit(EXIT_FAILURE);
    }

    // Crear directorio del servidor
    char comando_mkdir[100];
    sprintf(comando_mkdir, "mkdir -p %s", nombre_servidor);
    system(comando_mkdir);

    printf("Iniciando servidor %s en puerto %d...\n", nombre_servidor, PUERTO_BASE);
    guardar_log(nombre_servidor, "Servidor iniciado");

    // Crear socket principal
    if ((servidor_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Permitir reuso de dirección (importante para múltiples conexiones)
    int yes = 1;
    if (setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("Error en setsockopt");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Configurar dirección
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(PUERTO_BASE);

    // Enlazar socket
    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error al enlazar socket");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(servidor_fd, MAX_CLIENTES) < 0) {
        perror("Error al escuchar");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor %s escuchando en puerto %d\n", nombre_servidor, PUERTO_BASE);
    printf("Directorio: ~/%s/\n", nombre_servidor);
    printf("Logs: server_%s.log\n", nombre_servidor);
    printf("Esperando conexiones...\n\n");
    
    guardar_log(nombre_servidor, "Escuchando conexiones en puerto 49200");

    while (1) {
        // Aceptar conexión
        socklen_t tam_direccion = sizeof(direccion);
        if ((nuevo_socket = accept(servidor_fd, (struct sockaddr *)&direccion, &tam_direccion)) < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        // Recibir datos del cliente
        int bytes_recibidos = recv(nuevo_socket, buffer, TAM_BUFFER - 1, 0);
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
            
            // Verificar si es un status check
            if (strstr(buffer, "STATUS_CHECK") != NULL) {
                printf("Status check recibido de cliente\n");
                manejar_status_check(nuevo_socket, nombre_servidor);
                
                char log_msg[100];
                sprintf(log_msg, "Status check recibido: %.50s", buffer);
                guardar_log(nombre_servidor, log_msg);
                
            } else {
                // Es un archivo normal
                printf("Archivo recibido en %s\n", nombre_servidor);
                manejar_archivo(nuevo_socket, nombre_servidor, buffer);
            }
        } else {
            printf("Error: No se recibieron datos\n");
            send(nuevo_socket, "ERROR_SIN_DATOS", 15, 0);
            guardar_log(nombre_servidor, "Error: Cliente no envió datos");
        }

        close(nuevo_socket);
    }

    close(servidor_fd);
    return 0;
}