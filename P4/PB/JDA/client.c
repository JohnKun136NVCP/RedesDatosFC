#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>

// NUEVA FUNCIÓN PARA REGISTRAR ESTADOS
void log_status(const char *status) {
    FILE *log_file = fopen("status_log.txt", "a");
    if (log_file == NULL) {
        perror("No se pudo abrir el archivo de log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buffer[128];
    strftime(time_buffer, sizeof(time_buffer) - 1, "%Y-%m-%d %H:%M:%S", t);

    fprintf(log_file, "%s - %s\n", time_buffer, status);
    printf("[LOG] %s - %s\n", time_buffer, status);

    fclose(log_file);
}

char *leerArchivo(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *buf = malloc(sz + 1);
    if (!buf) {
        perror("malloc");
        fclose(f);
        return NULL;
    }

    size_t leidos = fread(buf, 1, sz, f);
    fclose(f);

    if (leidos != (size_t)sz) {
        fprintf(stderr, "Error al leer el archivo\n");
        free(buf);
        return NULL;
    }
    
    buf[leidos] = '\0';
    *len_out = leidos;
    return buf;
}

int main(int argc, char *argv[]) {
    // Ahora el alias que ponemos es al que no le vamos a mandar nada
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ALIAS_A_EXCLUIR> <PUERTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    const char *alias_excluido = argv[1];
    int puerto = atoi(argv[2]);
    const char *archivo = argv[3];

    // --- MODIFICACION RESPECTO A LA PARTE A --
    // Definimos la lista de todos los servidores posibles.
    const char *todos_los_servidores[] = {"s01", "s02", "s03", "s04"};
    int num_servidores = sizeof(todos_los_servidores) / sizeof(todos_los_servidores[0]);

    // Leemos el contenido del archivo una sola vez, antes de empezar a conectar.
    size_t text_len;
    char *texto = leerArchivo(archivo, &text_len);
    if (!texto) {
        return 1;
    }

    // Iteramos sobre la lista de todos los servidores.
    for (int i = 0; i < num_servidores; i++) {
        const char *alias_servidor_actual = todos_los_servidores[i];

        // Comprobamos si el servidor actual es el que debemos excluir.
        if (strcmp(alias_servidor_actual, alias_excluido) == 0) {
            printf("Omitiendo el servidor %s (excluido por argumento).\n", alias_servidor_actual);
            continue; // Si es el excluido, saltamos a la siguiente iteracion.
        }

        printf("\n--- Intentando conectar con el servidor: %s ---\n", alias_servidor_actual);

        int cliente_fd;
        struct sockaddr_in direccion;

        // Creamos un nuevo socket para cada conexion.
        cliente_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (cliente_fd < 0) {
            perror("Error al crear socket");
            log_status("Estado: FALLO al crear el socket.");
            continue; // Si falla, continuamos con el siguiente servidor.
        }

        direccion.sin_family = AF_INET;
        direccion.sin_port = htons(puerto);

        struct hostent *host_info = gethostbyname(alias_servidor_actual);
        if (host_info == NULL) {
            fprintf(stderr, "Error: No se pudo resolver el host '%s'\n", alias_servidor_actual);
            close(cliente_fd);
            continue; // Si no se resuelve el host, cerramos el socket y continuamos.
        }
        direccion.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);
        
        char status_msg[1280];

        // Intentamos conectar al servidor actual.
        if (connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
            perror("Error en connect");
            snprintf(status_msg, sizeof(status_msg), "Estado: FALLO al conectar con %s", alias_servidor_actual);
            log_status(status_msg);
            close(cliente_fd);
            continue; // Si falla la conexion, cerramos el socket y continuamos.
        }
        
        snprintf(status_msg, sizeof(status_msg), "Estado: Conectado a %s. Enviando archivo '%s'.", alias_servidor_actual, archivo);
        log_status(status_msg);

        const char *nombre_base = strrchr(archivo, '/');
        if (nombre_base == NULL) nombre_base = archivo;
        else nombre_base++;
        
        // Enviamos el nombre y el contenido del archivo.
        send(cliente_fd, nombre_base, strlen(nombre_base), 0);
        sleep(1);
        send(cliente_fd, texto, text_len, 0);
        
        char buffer_respuesta[1024] = {0};
        int bytes_recibidos = recv(cliente_fd, buffer_respuesta, sizeof(buffer_respuesta) - 1, 0);
        if (bytes_recibidos > 0) {
            buffer_respuesta[bytes_recibidos] = '\0';
            snprintf(status_msg, sizeof(status_msg), "Respuesta de %s: %s", alias_servidor_actual, buffer_respuesta);
            log_status(status_msg);
        } else {
            snprintf(status_msg, sizeof(status_msg), "Estado: Conexión con %s cerrada sin respuesta final.", alias_servidor_actual);
            log_status(status_msg);
        }

        // Cerramos el descriptor del socket para esta conexión.
        close(cliente_fd);
    }

    // Liberamos la memoria del archivo después de haber terminado con todos los servidores.
    free(texto);
    printf("\nProceso completado para todos los servidores.\n");

    return 0;
}
