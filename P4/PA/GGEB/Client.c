#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF 4096

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <HOST(s01|s02|IP)> <PUERTO> <archivo>\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];   
    const char *port = argv[2];   
    const char *file = argv[3]; 

    FILE *f = fopen(file, "rb");
    if (!f) { perror("archivo"); return 1; }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;     
    hints.ai_socktype = SOCK_STREAM; 

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        fclose(f);
        return 1;
    }

    // Creo el socket según la dirección resuelta
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { perror("socket"); freeaddrinfo(res); fclose(f); return 1; }

    // Intento conectar al servidor y si no hay ruta o no está escuchando, fallará
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(s); freeaddrinfo(res); fclose(f);
        return 1;
    }
    // Ya no necesito la info de resolución
    freeaddrinfo(res);

    // Envío el archivo en bloques de BUF bytes hasta EOF
    char    buf[BUF];
    size_t  n;          // bytes leídos del archivo
    size_t  total = 0;  // contador de bytes enviados, para imprimir al final
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        // send puede enviar menos de lo pedido, en este caso, por simplicidad,
        ssize_t m = send(s, buf, n, 0);
        if (m < 0) { perror("send"); close(s); fclose(f); return 1; }
        total += (size_t)m;
    }
    fclose(f);  // cierro el archivo local

    // cierro la escritura del socket para que el servidor vea EOF
    shutdown(s, SHUT_WR);
    // Y cierro completamente el socket (no espero respuesta en este cliente simple)
    close(s);

    // Mensaje para consola
    printf("[CLIENTE] Enviados %zu bytes a %s:%s\n", total, host, port);
    return 0;
}
