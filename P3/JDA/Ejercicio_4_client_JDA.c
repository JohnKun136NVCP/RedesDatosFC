#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

// Las funciones auxiliares readn, writen y leerArchivo no cambian.
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    char *ptr = buf;
    while (left > 0) {
        ssize_t r = read(fd, ptr, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            break;
        }
        left -= r;
        ptr += r;
    }
    return n - left;
}

ssize_t writen(int fd, const void *buf, size_t n) {
    size_t left = n;
    const char *ptr = buf;
    while (left > 0) {
        ssize_t w = write(fd, ptr, left);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            return -1;
        }
        left -= w;
        ptr += w;
    }
    return n;
}

char *leerArchivo(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *buf = malloc(sz);
    if (!buf) { perror("malloc"); fclose(f); return NULL; }

    size_t leidos = fread(buf, 1, sz, f);
    fclose(f);

    if (leidos != (size_t)sz) {
        fprintf(stderr, "Error al leer archivo\n");
        free(buf);
        return NULL;
    }

    *len_out = sz;
    return buf;
}

int main(int argc, char *argv[]) {
    // Validación de argumentos: 1 (programa) + 1 (IP) + k (puertos) + k (archivos) + 1 (shift)
    // El total de argumentos debe ser 2k + 3, por lo que (argc - 3) debe ser par y positivo.
    if (argc < 5 || (argc - 3) % 2 != 0) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <puerto1> ... <puertoN> <archivo1> ... <archivoN> <DESPLAZAMIENTO>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 49200 49201 mi_archivo1.txt mi_archivo2.txt 10\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int shift = atoi(argv[argc - 1]);
    int num_pares = (argc - 3) / 2;

    // Bucle principal para procesar cada par puerto-archivo
    for (int i = 0; i < num_pares; i++) {
        int target_port = atoi(argv[2 + i]);
        const char *archivo_path = argv[2 + num_pares + i];
        
        printf("\n--- Procesando Puerto %d con Archivo '%s' ---\n", target_port, archivo_path);

        size_t text_len;
        char *texto = leerArchivo(archivo_path, &text_len);
        if (!texto) {
            fprintf(stderr, "No se pudo leer el archivo %s, saltando...\n", archivo_path);
            continue;
        }

        int cliente_fd;
        struct sockaddr_in direccion;

        // Crear socket
        cliente_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (cliente_fd < 0) {
            perror("socket");
            free(texto);
            continue;
        }

        // Configurar dirección del servidor al que nos conectaremos
        direccion.sin_family = AF_INET;
        direccion.sin_port = htons(target_port); // Conectamos directamente al puerto objetivo
        inet_pton(AF_INET, ip, &direccion.sin_addr);

        // Conectar al servidor
        if (connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
            perror("connect");
            close(cliente_fd);
            free(texto);
            continue;
        }

        // Armar y enviar el mensaje (puerto objetivo, desplazamiento, longitud y contenido del archivo)
        uint32_t net_target_port = htonl(target_port);
        uint32_t net_shift = htonl(shift);
        uint32_t net_len = htonl(text_len);

        writen(cliente_fd, &net_target_port, 4);
        writen(cliente_fd, &net_shift, 4);
        writen(cliente_fd, &net_len, 4);
        if (text_len > 0) {
            writen(cliente_fd, texto, text_len);
        }

        // Recibir respuesta del servidor
        uint32_t net_status;
        if (readn(cliente_fd, &net_status, 4) != 4) {
            perror("read status");
        } else {
             uint32_t status = ntohl(net_status);
             if (status == 1) { // PROCESADO
                 printf("[Cliente] Puerto %d: ARCHIVO PROCESADO Y RECIBIDO\n", target_port);
                 // Opcional: leer el contenido cifrado si el servidor lo devuelve
             } else { // RECHAZADO
                 printf("[Cliente] Puerto %d: SOLICITUD RECHAZADA\n", target_port);
             }
        }
        
        close(cliente_fd);
        free(texto);
    }

    return 0;
}
