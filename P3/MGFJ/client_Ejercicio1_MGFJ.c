#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 4096

// Lee una línea desde un socket
static ssize_t read_line(int fd, char *out, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        char c; ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) { if (i == 0) return 0; break; } // conexión cerrada
        if (r < 0) { if (errno == EINTR) continue; return -1; } // error
        out[i++] = c; if (c == '\n') break; // fin de línea
    }
    out[i] = '\0'; return (ssize_t)i;
}

// Obtiene el nombre base de un archivo (sin ruta)
static const char* basename_simple(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

// Conexión al servidor TCP
static int connect_to(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("[-] socket"); return -1; }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET; sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) <= 0) { perror("[-] inet_pton"); close(fd); return -1; }
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { perror("[-] connect"); close(fd); return -1; }
    return fd;
}

// Envia archivos al servidor
static int send_file(int fd, const char *path) {
    FILE *fp = fopen(path, "rb"); if (!fp) { perror("[-] fopen"); return -1; }
    char buf[BUFFER_SIZE]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send(fd, buf, n, 0) < 0) { perror("[-] send"); fclose(fp); return -1; }
    }
    fclose(fp); 
    shutdown(fd, SHUT_WR); 
    
    return 0;
}

int main(int argc, char *argv[]) {
    // Uso esperado
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <SERVER_IP> <PORT> <SHIFT> <FILE>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *file_path = argv[4];
    const char *file_name = basename_simple(file_path);

    const int server_ports[3] = {49200, 49201, 49202};

    // Intenta conectarse a cada servidor
    for (int i = 0; i < 3; ++i) {
        int srv_port = server_ports[i];
        int fd = connect_to(server_ip, srv_port);
        if (fd < 0) {
            printf("[-] SERVER RESPONSE %d: no se pudo conectar\n", srv_port);
            continue;
        }

        // Envia la cabecera con parámetros
        char header[1024];
        int hdr = snprintf(header, sizeof(header),
                           "TARGET %d\nSHIFT %d\nNAME %s\n\n",
                           port, shift, file_name);
        if (send(fd, header, hdr, 0) < 0) {
            printf("[-] SERVER RESPONSE %d: error de header\n", srv_port);
            close(fd); continue;
        }
        if (send_file(fd, file_path) != 0) {
            printf("[-] SERVER RESPONSE %d: error enviando archivo\n", srv_port);
            close(fd); continue;
        }

        // Lee la respuesta del servidor
        char line[1024];
        ssize_t rl = read_line(fd, line, sizeof(line));
        if (rl <= 0) {
            printf("[-] SERVER RESPONSE %d: sin respuesta\n", srv_port);
            close(fd); continue;
        }
        if (strncmp(line, "REJECT", 6) == 0) {
            printf("[-] SERVER RESPONSE %d: REJECTED\n", srv_port);
            close(fd); continue;
        }
        if (strncmp(line, "OK", 2) != 0) {
            printf("[-] SERVER RESPONSE %d: respuesta inválida\n", srv_port);
            close(fd); continue;
        }

        // Recibe el nombre del archivo
        char nameline[1024];
        rl = read_line(fd, nameline, sizeof(nameline));
        if (rl <= 0) { printf("[-] SERVER RESPONSE %d: sin NAME\n", srv_port); close(fd); continue; }

        char recv_name[512] = {0};
        if (sscanf(nameline, "NAME %511[^\n]", recv_name) != 1) {
            printf("[-] SERVER RESPONSE %d: NAME inválido\n", srv_port);
            close(fd); continue;
        }

        // Consumir línea en blanco
        char blank[8]; read_line(fd, blank, sizeof(blank));

        // Guardar archivo recibido
        FILE *out = fopen(recv_name, "wb");
        if (!out) { perror("[-] fopen salida"); close(fd); continue; }

        char buf[BUFFER_SIZE]; ssize_t n;
        while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) fwrite(buf, 1, (size_t)n, out);
        fclose(out);

        printf("[+] SERVER RESPONSE %d: File received and encrypted.\n", srv_port);
        close(fd);
    }

    return 0;
}
