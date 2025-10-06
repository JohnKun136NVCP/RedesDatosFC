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

    if (argc != 3 && argc != 4) {
        // ./client_pb <HOST> <PUERTO> <archivo>
    }
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <HOST> <PUERTO> <archivo>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];  // puede ser s01/s02/s03/s04 o una IP
    const char *port = argv[2];  // puerto destino
    const char *file = argv[3];  // ruta del archivo a enviar

    // Abro el archivo en modo binario y si no existe o no hay permisos, fallo aquí
    FILE *f = fopen(file, "rb");
    if (!f) { perror("archivo"); return 1; }

    // Preparo resolución del <HOST>:<PUERTO> a una dirección IPv4
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // solo IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0 || !res) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        fclose(f);
        return 1;
    }

    // Creo el socket TCP y me conecto al servidor/alias indicado.
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { perror("socket"); freeaddrinfo(res); fclose(f); return 1; }

    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");        // si no hay server escuchando o hay ruta mal, se ve aquí
        close(s); freeaddrinfo(res); fclose(f);
        return 1;
    }
    freeaddrinfo(res);           

    // leo del archivo en bloques BUF y mando por el socket.
    char buf[BUF];
    size_t n, total = 0;          // 'total' acumula bytes enviados
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        ssize_t m = send(s, buf, n, 0);
        if (m < 0) { perror("send"); close(s); fclose(f); return 1; }
        total += (size_t)m;
    }
    fclose(f);                    // cierro el archivo local

    // Indico al servidor que ya no enviaré más datos y cierro el socket.
    shutdown(s, SHUT_WR);
    close(s);

    // Mensaje final en consola con el conteo de bytes y destino.
    printf("[CLIENTE] Enviados %zu bytes a %s:%s\n", total, host, port);
    return 0;
}

