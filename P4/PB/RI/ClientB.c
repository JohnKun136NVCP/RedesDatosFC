#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Conecto TCP a host:port
static int conectar_tcp(const char *nombre_host, const char *texto_puerto) {
    struct addrinfo pistas, *res = NULL;
    memset(&pistas, 0, sizeof(pistas));
    pistas.ai_family   = AF_INET;      
    pistas.ai_socktype = SOCK_STREAM; 

    if (getaddrinfo(nombre_host, texto_puerto, &pistas, &res) != 0) return -1;

    int fd_con = -1;
    for (struct addrinfo *it = res; it; it = it->ai_next) {
        fd_con = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd_con < 0) continue;
        if (connect(fd_con, it->ai_addr, it->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd_con; 
        }
        close(fd_con);
        fd_con = -1;
    }
    freeaddrinfo(res);
    return -1;
}

// Lee una línea terminada en '\n' desde fd y la guarda sin el '\n'.
// Devuelve bytes leídos o -1 si falla.
static int leer_linea(int fd, char *buf, size_t max) {
    size_t i = 0;
    char c;
    while (i + 1 < max) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return -1;       // error o conexión cerrada
        if (c == '\n') { buf[i] = '\0'; return (int)i; }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i; 
}

int main(int argc, char **argv) {
    // Validación básica de argumentos
    if (argc < 5) {
        fprintf(stderr,
                "Uso:\n  %s <host> <PORT_BASE> status <log>\n"
                "  %s <host> <PORT_BASE> put <archivo>\n",
                argv[0], argv[0]);
        return 1;
    }

    
    const char *alias_servidor = argv[1]; 
    const char *puerto_base    = argv[2]; 
    const char *modo_operacion = argv[3]; 
    const char *argumento      = argv[4]; 

    // El server me dará el puerto efímero
    int fd_handshake = conectar_tcp(alias_servidor, puerto_base);
    if (fd_handshake < 0) { perror("connect base"); return 1; }

    char linea[256];
    if (leer_linea(fd_handshake, linea, sizeof(linea)) < 0) {
        fprintf(stderr, "Error en handshake\n");
        close(fd_handshake);
        return 1;
    }
    close(fd_handshake);

    unsigned puerto_efimero = 0;
    if (sscanf(linea, "PORT %u", &puerto_efimero) != 1) {
        fprintf(stderr, "Protocolo incorrecto (esperaba 'PORT <num>'): %s\n", linea);
        return 1;
    }

    char puerto_ef_str[16];
    snprintf(puerto_ef_str, sizeof(puerto_ef_str), "%u", puerto_efimero);

    // Abre la conexión real de trabajo al puerto efímero
    int fd_trabajo = conectar_tcp(alias_servidor, puerto_ef_str);
    if (fd_trabajo < 0) { perror("connect eph"); return 1; }

    // pregunta si el servidor está libre o recibiendo
    if (strcmp(modo_operacion, "status") == 0) {
        const char *consulta = "STATUS\n";
        send(fd_trabajo, consulta, strlen(consulta), 0);

        if (leer_linea(fd_trabajo, linea, sizeof(linea)) < 0) {
            close(fd_trabajo);
            return 1;
        }
        close(fd_trabajo);

        // Registro en log con timestamp "YYYY-MM-DD HH:MM:SS ESTADO"
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        char marca_tiempo[32];
        strftime(marca_tiempo, sizeof(marca_tiempo), "%F %T", lt);

        FILE *arch_log = fopen(argumento, "a");
        if (!arch_log) { perror("logfile"); return 1; }
        fprintf(arch_log, "%s %s\n", marca_tiempo, linea);
        fclose(arch_log);

       
        printf("[CLIENTE] %s %s\n", marca_tiempo, linea);
        return 0;
    }

    // Envía archivo al servidor usando el mini-protocolo
    if (strcmp(modo_operacion, "put") == 0) {
        const char *ruta_archivo = argumento;
        FILE *fp = fopen(ruta_archivo, "rb");
        if (!fp) { perror("archivo"); close(fd_trabajo); return 1; }

        struct stat st;
        if (stat(ruta_archivo, &st) < 0) { perror("stat"); fclose(fp); close(fd_trabajo); return 1; }

        // Nombre base del archivo
        const char *nombre_base = strrchr(ruta_archivo, '/');
        nombre_base = nombre_base ? nombre_base + 1 : ruta_archivo;

        // Encabezado del protocolo
        char encabezado[512];
        snprintf(encabezado, sizeof(encabezado), "PUT %s\n%lld\n",
                 nombre_base, (long long)st.st_size);
        send(fd_trabajo, encabezado, strlen(encabezado), 0);

        // Envía el archivo por bloques
        char buffer_envio[8192];
        size_t leidos;
        while ((leidos = fread(buffer_envio, 1, sizeof(buffer_envio), fp)) > 0) {
            if (send(fd_trabajo, buffer_envio, leidos, 0) < 0) { perror("send"); break; }
        }
        fclose(fp);

        // Espera confirmación del servidor
        if (leer_linea(fd_trabajo, linea, sizeof(linea)) > 0)
            printf("[CLIENTE] %s\n", linea);

        close(fd_trabajo);
        return 0;
    }

    fprintf(stderr, "Modo desconocido: %s\n", modo_operacion);
    close(fd_trabajo);
    return 1;
}