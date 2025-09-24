#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>

//NUEVA FUNCIÓN PARA REGISTRAR ESTADOS
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
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ALIAS_SERVIDOR> <PUERTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    const char *alias_servidor = argv[1];
    int puerto = atoi(argv[2]);
    const char *archivo = argv[3];

    size_t text_len;
    char *texto = leerArchivo(archivo, &text_len);
    if (!texto) {
        return 1;
    }

    int cliente_fd;
    struct sockaddr_in direccion;

    cliente_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente_fd < 0) {
        perror("socket");
        free(texto);
        return 1;
    }

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);

    struct hostent *host_info = gethostbyname(alias_servidor);
    if (host_info == NULL) {
        fprintf(stderr, "Error: No se pudo resolver el host '%s'\n", alias_servidor);
        free(texto);
        close(cliente_fd);
        return 1;
    }
    direccion.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);

    
    char status_msg[1280];

    if (connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("connect");
        snprintf(status_msg, sizeof(status_msg), "Estado: FALLO al conectar con %s", alias_servidor);
        log_status(status_msg);
        free(texto);
        close(cliente_fd);
        return 1;
    }
    
    snprintf(status_msg, sizeof(status_msg), "Estado: Conectado a %s. Enviando archivo '%s'.", alias_servidor, archivo);
    log_status(status_msg);

    const char *nombre_base = strrchr(archivo, '/');
    if (nombre_base == NULL) nombre_base = archivo;
    else nombre_base++;
    
    send(cliente_fd, nombre_base, strlen(nombre_base), 0);
    sleep(1);
    send(cliente_fd, texto, text_len, 0);
    
    char buffer_respuesta[1024] = {0};
    int bytes_recibidos = recv(cliente_fd, buffer_respuesta, sizeof(buffer_respuesta) - 1, 0);
    if (bytes_recibidos > 0) {
        buffer_respuesta[bytes_recibidos] = '\0';
        snprintf(status_msg, sizeof(status_msg), "Respuesta de %s: %s", alias_servidor, buffer_respuesta);
        log_status(status_msg);
    } else {
        snprintf(status_msg, sizeof(status_msg), "Estado: Conexión con %s cerrada sin respuesta final.", alias_servidor);
        log_status(status_msg);
    }

    free(texto);
    close(cliente_fd);

    return 0;
}
