#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BACKLOG 16     
#define BUFSZ   8192  

static volatile int servidor_ocupado = 0;

// Función auxiliar: crea socket TCP IPv4, bindea y pone a escuchar en 'puerto'
static int bind_y_escuchar(uint16_t puerto) {
    int fd_escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_escucha < 0) return -1;

    int opt = 1;
    setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dir_srv;
    memset(&dir_srv, 0, sizeof(dir_srv));
    dir_srv.sin_family      = AF_INET;
    dir_srv.sin_addr.s_addr = INADDR_ANY;         // Acepto en cualquier interfaz
    dir_srv.sin_port        = htons(puerto);

    if (bind(fd_escucha, (struct sockaddr*)&dir_srv, sizeof(dir_srv)) < 0) {
        close(fd_escucha);
        return -1;
    }
    if (listen(fd_escucha, BACKLOG) < 0) {
        close(fd_escucha);
        return -1;
    }
    return fd_escucha;
}

// Recorro puertos desde inicio hacia arriba buscando uno libre para escuchar.
// Devuelvo fd de escucha y escribo el puerto elegido en puerto_out
static int reservar_puerto_por_encima(uint16_t inicio, uint16_t *puerto_out) {
    for (uint32_t p = inicio; p <= 60000; ++p) {
        int fd_escucha = bind_y_escuchar((uint16_t)p);
        if (fd_escucha >= 0) { *puerto_out = (uint16_t)p; return fd_escucha; }
    }
    return -1;
}

// Acepta una conexión y devuelve el fd del socket conectado 
static int aceptar_uno(int fd_escucha) {
    struct sockaddr_in dir_cli;
    socklen_t tam = sizeof(dir_cli);
    int fd_cli = accept(fd_escucha, (struct sockaddr*)&dir_cli, &tam);
    return fd_cli;
}

// Recibe una línea terminada en '\n' . Coloca '\0' al final.
// Devuelve bytes leídos (>=0) o <=0 si error/cierre.
static ssize_t recibir_linea(int fd, char *buf, size_t max) {
    size_t i = 0;
    char c;
    while (i + 1 < max) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;       // error o cierre
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

// Asegura que exista ~/subdir 
static int asegurar_directorio(const char *subdir) {
    char ruta[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(ruta, sizeof(ruta), "%s/%s", home, subdir);

    struct stat st;
    if (stat(ruta, &st) == -1) {
        if (mkdir(ruta, 0700) == -1) { perror("mkdir"); return -1; }
    }
    return 0;
}

// Abre subdir/nombre para escribir en binario
static FILE *abrir_destino(const char *subdir, const char *nombre) {
    char ruta[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(ruta, sizeof(ruta), "%s/%s/%s", home, subdir, nombre);
    return fopen(ruta, "wb");
}

int main(int argc, char **argv) {
    // Validación básica: ./ServerA puerto_base dir_servidor
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <puerto_base> <dir_servidor>\n", argv[0]);
        return 1;
    }
    uint16_t puerto_base = (uint16_t)atoi(argv[1]);  
    const char *dir_servidor = argv[2];              

    if (puerto_base == 0) {
        fprintf(stderr, "Error: puerto_base no válido\n");
        return 1;
    }

    // Asegura de que exista s01 o s02 según alias con el que se corra el servidor
    if (asegurar_directorio(dir_servidor) != 0) return 1;

    // Escucha en el puerto base
    int fd_escucha_base = bind_y_escuchar(puerto_base);
    if (fd_escucha_base < 0) {
        perror("bind/listen puerto base");
        return 1;
    }
    fprintf(stderr, "[SERVIDOR] Escuchando puerto base %u para handshake.\n", puerto_base);

    // Creo un buffer para recibir datos de archivo.
    char *buffer_grande = (char*)malloc(1 << 20); 
    if (!buffer_grande) {
        fprintf(stderr, "Error: no hay memoria\n");
        close(fd_escucha_base);
        return 1;
    }

    for (;;) {
        //  acepta en el puerto base
        int fd_cli_handshake = aceptar_uno(fd_escucha_base);
        if (fd_cli_handshake < 0) {
            perror("accept handshake");
            continue;
        }

        // Asigno un puerto efímero >49200
        uint16_t puerto_efimero = 0;
        int fd_escucha_efimero = reservar_puerto_por_encima(49201, &puerto_efimero);
        if (fd_escucha_efimero < 0) {
            fprintf(stderr, "No hay puerto efímero disponible.\n");
            close(fd_cli_handshake);
            continue;
        }

        // Le informo al cliente a qué puerto ir para continuar.
        char mensaje_puerto[64];
        int len = snprintf(mensaje_puerto, sizeof(mensaje_puerto), "PORT %u\n", puerto_efimero);
        send(fd_cli_handshake, mensaje_puerto, (size_t)len, 0);
        close(fd_cli_handshake);

        //Acepto la segunda conexión en el puerto efímero para STATUS o PUT.
        int fd_cli = aceptar_uno(fd_escucha_efimero);
        close(fd_escucha_efimero);
        if (fd_cli < 0) {
            perror("accept efimero");
            continue;
        }

        // Leo la primera línea 
        // Formatos esperados:
        //  - "STATUS\n"
        //  - "PUT <nombre>\n"  y luego línea con el tamaño en bytes "<entero>\n"
        char linea[1024];
        ssize_t n = recibir_linea(fd_cli, linea, sizeof(linea));
        if (n <= 0) { close(fd_cli); continue; }

        //  responde según la bandera global.
        if (strncmp(linea, "STATUS", 6) == 0) {
            const char *respuesta = servidor_ocupado ? "RECIBIENDO\n" : "ESPERA\n";
            send(fd_cli, respuesta, strlen(respuesta), 0);
            close(fd_cli);
            continue;
        }

        // 
        if (strncmp(linea, "PUT ", 4) == 0) {
            char nombre_archivo[256] = {0};
            // Extraigo el nombre del archivo
            sscanf(linea + 4, "%255s", nombre_archivo);

            // La siguiente línea debe traer el tamaño total a recibir
            if (recibir_linea(fd_cli, linea, sizeof(linea)) <= 0) {
                close(fd_cli);
                continue;
            }
            long long bytes_totales = atoll(linea);
            if (bytes_totales < 0) bytes_totales = 0; 

            // Abro destino en s01 o s02 según el alias que pasé 
            FILE *salida = abrir_destino(dir_servidor, nombre_archivo);
            if (!salida) {
                perror("fopen salida");
                close(fd_cli);
                continue;
            }

        
            servidor_ocupado = 1;
            long long acumulado = 0;

            while (acumulado < bytes_totales) {
                ssize_t r = recv(fd_cli, buffer_grande, BUFSZ, 0);
                if (r <= 0) break; 
                fwrite(buffer_grande, 1, (size_t)r, salida);
                acumulado += r;
            }

            fclose(salida);
            servidor_ocupado = 0;

            // Confirmo al cliente que terminé
            send(fd_cli, "OK\n", 3, 0);
            close(fd_cli);
            continue;
        }

        // Si no se reconoce el comando, cierro.
        close(fd_cli);
    }

    
    free(buffer_grande);
    close(fd_escucha_base);
    return 0;
}
