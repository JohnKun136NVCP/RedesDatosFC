#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUERTO_SERVIDOR 49200
#define BUFSZ 8192

// ======= Función para conectar al servidor usando alias/IP =======
static int conectar_por_alias(const char *alias, int puerto) {
    struct hostent *he = gethostbyname(alias);
    if (!he) {
        fprintf(stderr, "No se pudo resolver el alias: %s\n", alias);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(puerto);
    memcpy(&srv.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));

    if (connect(fd, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "USO: %s <alias> <desplazamiento> <archivo>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s s01 5 prueba.txt\n", argv[0]);
        return 1;
    }

    const char *alias = argv[1];
    int desplazamiento = atoi(argv[2]);
    const char *ruta   = argv[3];

    // ======= Leer archivo completo en memoria =======
    FILE *fp = fopen(ruta, "rb");
    if (!fp) { perror("fopen"); return 1; }

    char texto[BUFSZ];
    size_t n = fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);
    texto[n] = '\0';

    // ======= Intentar conexión al servidor en puerto fijo 49200 =======
    int fd = conectar_por_alias(alias, PUERTO_SERVIDOR);
    if (fd < 0) return 1;

    // ======= Armar mensaje con formato esperado =======
    // "<PUERTO_OBJETIVO> <DESPLAZAMIENTO> <TEXTO>"
    char mensaje[BUFSZ * 2];
    int m = snprintf(mensaje, sizeof(mensaje), "%d %d %s",
                     PUERTO_SERVIDOR, desplazamiento, texto);

    if (send(fd, mensaje, (size_t)m, 0) < 0) {
        perror("send");
        close(fd);
        return 1;
    }

    // Recibir respuesta
    char resp[BUFSZ * 2];
    int r = recv(fd, resp, sizeof(resp) - 1, 0);
    if (r < 0) {
        perror("recv");
        close(fd);
        return 1;
    }
    resp[r] = '\0';

    printf("[*]Respuesta del servidor %s (puerto %d): %s\n",
           alias, PUERTO_SERVIDOR, resp);

    close(fd);
    return 0;
}
