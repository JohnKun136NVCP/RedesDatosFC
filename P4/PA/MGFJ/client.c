#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CTRL_PORT 49200   // Puerto de control
#define BUFSZ     4096    // Tamaño de buffer para envío

// Escribe en status.log una línea con fecha, hora, estado, archivo y servidor.
static void log_estado(const char *estado_linea, const char *server, int data_port, const char *file) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char fecha[20], hora[20];
    strftime(fecha, sizeof(fecha), "%F", tm);
    strftime(hora,  sizeof(hora),  "%T", tm);

    char estado[256];
    snprintf(estado, sizeof(estado), "%s", estado_linea);
    char *nl = strchr(estado, '\n');
    if (nl) *nl = '\0';

    FILE *f = fopen("status.log", "a");
    if (!f) { perror("status.log"); return; }

    fprintf(f, "%s | %s | %s | %s | %s:%d\n",
            fecha, hora, estado, file, server, data_port);
    fclose(f);
}

// Abre el socket y conecta a ip:port (acepta literal o alias en /etc/hosts).
static int dial_ip(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);

    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        a.sin_addr.s_addr = inet_addr(ip);
        if (a.sin_addr.s_addr == INADDR_NONE) {
            fprintf(stderr, "IP/host inválido: %s\n", ip);
            exit(1);
        }
    }
    if (connect(fd, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("connect"); exit(1);
    }
    return fd;
}

// Recibe una línea terminada en '\n' del socket fd y la guarda en buf.
static ssize_t recv_line(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <SERVER_IP|HOST> <ARCHIVO>\n", argv[0]);
        return 1;
    }
    const char *server = argv[1];
    const char *file   = argv[2];

    // Paso 1: conectar al puerto de control y recibir "PORT X"
    int cfd = dial_ip(server, CTRL_PORT);
    char line[256];
    ssize_t n = recv_line(cfd, line, sizeof(line));
    if (n <= 0) {
        fprintf(stderr, "No recibí puerto de datos\n");
        close(cfd);
        return 1;
    }
    close(cfd);

    int data_port = 0;
    if (sscanf(line, "PORT %d", &data_port) != 1 || data_port <= CTRL_PORT) {
        fprintf(stderr, "Respuesta de control inválida: %s", line);
        return 1;
    }
    fprintf(stderr, "[CLIENT] Puerto de datos asignado: %d\n", data_port);

    // Paso 2: conectar al puerto de datos
    int dfd = dial_ip(server, data_port);

    // Paso 3: leer estados iniciales y registrarlos
    for (int i = 0; i < 2; ++i) {
        n = recv_line(dfd, line, sizeof(line));
        if (n <= 0) {
            fprintf(stderr, "Conexión cerrada antes de tiempo\n");
            close(dfd);
            return 1;
        }
        fputs(line, stdout);
        log_estado(line, server, data_port, file);
    }

    // Paso 4: enviar archivo completo al servidor
    FILE *fp = fopen(file, "rb");
    if (!fp) {
        perror("fopen");
        close(dfd);
        return 1;
    }
    char buf[BUFSZ]; size_t rn;
    while ((rn = fread(buf, 1, sizeof(buf), fp)) > 0) {
        ssize_t sent = send(dfd, buf, rn, 0);
        if (sent < 0) {
            perror("send");
            fclose(fp);
            close(dfd);
            return 1;
        }
    }
    fclose(fp);
    shutdown(dfd, SHUT_WR);

    // Paso 5: leer estados finales (TRANSMITIENDO, OK, etc.)
    while ((n = recv_line(dfd, line, sizeof(line))) > 0) {
        fputs(line, stdout);
        log_estado(line, server, data_port, file);
    }

    close(dfd);
    return 0;
}
