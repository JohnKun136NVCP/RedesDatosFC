#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#define TAM_BUF 8192

// Conecta por TCP a puerto. Devuelve descriptor de socket o -1 en error.
static int conectar_tcp(const char *host, int puerto) {
    int fd = -1;
    struct addrinfo pistas, *res = NULL, *it;
    char puerto_str[16];

    // Preparamos IPv4 + TCP
    snprintf(puerto_str, sizeof(puerto_str), "%d", puerto);
    memset(&pistas, 0, sizeof(pistas));
    pistas.ai_family   = AF_INET;
    pistas.ai_socktype = SOCK_STREAM; // TCP

    // Resolvemos:puerto a una o más direcciones
    if (getaddrinfo(host, puerto_str, &pistas, &res) != 0) return -1;

    // Intentamos conectar con la primera dirección que funcione
    for (it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd);
        fd = -1; // si falló, probamos con la siguiente
    }
    freeaddrinfo(res);
    return fd;
}

// Lee todo el archivo en memoria y lo retorna como cadena.
static char *leer_archivo(const char *ruta, size_t *out_len) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;

    // Medimos tamaño
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);

    // Reserva memoria y lee
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t leidos = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[leidos] = '\0';          // teminacion del string
    if (out_len) *out_len = leidos;
    return buf;
}

// Envía exactamente n bytes sobre el socket
static int enviar_todo(int fd, const char *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t k = send(fd, buf + off, n - off, 0);
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    return 0;
}

// Arma el encabezado del protocolo y envía el archivo.
// Luego hace shutdown de escritura para indicar fin de datos y cierra el socket.
static void habla_con_servidor(const char *host, int puerto_servidor,
                               int puerto_objetivo, int desplazamiento,
                               const char *contenido) {
    int fd = conectar_tcp(host, puerto_servidor);
    if (fd < 0) {
        printf("[CLIENTE] No conectó con %s:%d\n", host, puerto_servidor);
        return;
    }

    // PORT <puerto_objetivo>\n
    // SHIFT <desplazamiento>\n
    // \n
    char encabezado[128];
    int m = snprintf(encabezado, sizeof(encabezado),
                     "PORT %d\nSHIFT %d\n\n", puerto_objetivo, desplazamiento);

    // Enviamos encabezado y luego el contenido completo del archivo
    if (enviar_todo(fd, encabezado, (size_t)m) != 0 ||
        enviar_todo(fd, contenido, strlen(contenido)) != 0) {
        printf("[CLIENTE] Error al enviar a %s:%d\n", host, puerto_servidor);
        close(fd);
        return;
    }

    // Avisamos fin de envío
    shutdown(fd, SHUT_WR);
    close(fd);
    printf("[CLIENTE] Datos enviados a %s:%d\n", host, puerto_servidor);
}

// Verifica el puerto
static int puerto_permitido(int p) {
    return (p == 49200 || p == 49201 || p == 49202);
}

int main(int argc, char **argv) {
    // Validación
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <host> <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <archivo.txt>\n", argv[0]);
        return 1;
    }

    // Parámetros de entrada
    const char *host = argv[1];
    int puerto_objetivo = atoi(argv[2]);
    int desplazamiento  = atoi(argv[3]);
    const char *ruta_archivo = argv[4];

    if (!puerto_permitido(puerto_objetivo)) {
        fprintf(stderr, "PUERTO_OBJETIVO debe ser 49200, 49201 o 49202.\n");
        return 1;
    }

    // Leemos el archivo a memoria
    size_t tam = 0;
    char *contenido = leer_archivo(ruta_archivo, &tam);
    if (!contenido) {
        perror("No se pudo leer el archivo");
        return 1;
    }

    // Lista de puertos a los que hay que conectarse
    int puertos[3] = {49200, 49201, 49202};
    for (int i = 0; i < 3; i++) {
        habla_con_servidor(host, puertos[i], puerto_objetivo, desplazamiento, contenido);
    }

    // Limpieza de memoria
    free(contenido);
    return 0;
}
