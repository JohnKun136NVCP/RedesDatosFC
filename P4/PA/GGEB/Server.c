#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define BUF 4096
#define S01_IP "192.168.0.101"
#define S02_IP "192.168.0.102"

static void ensure_dir(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
    }
}

int main(int argc, char *argv[]) {
    // Valido que me pasen el puerto como argumento
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\nEj.: %s 49200\n", argv[0], argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);  // convierto el argumento a entero

    // Creo el socket de escucha (IPv4, TCP)
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;                        
    addr.sin_port        = htons((unsigned short)port);   
    addr.sin_addr.s_addr = INADDR_ANY;                     

    // Enlazo el socket al puerto/addr
    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(ls); return 1; }
    // Pongo el socket en modo escucha con cola de 10 conexiones pendientes
    if (listen(ls, 10) < 0)                       { perror("listen"); close(ls); return 1; }

    printf("[SERVER] escuchando en puerto %d ...\n", port);

    const char *home = getenv("HOME"); if (!home) home = ".";  
    char dir_s01[256], dir_s02[256];
    snprintf(dir_s01, sizeof(dir_s01), "%s/s01", home);
    snprintf(dir_s02, sizeof(dir_s02), "%s/s02", home);
    ensure_dir(dir_s01);
    ensure_dir(dir_s02);

    // Bucle para aceptar conexiones una por una y procesarlas
    for (;;) {
        struct sockaddr_in cli;            // info del cliente (no la uso mucho aquí)
        socklen_t clen = sizeof(cli);
        int c = accept(ls, (struct sockaddr*)&cli, &clen);  // bloqueo hasta que llegue un cliente
        if (c < 0) { perror("accept"); continue; }

        // Quiero saber a que IP local llegó esta conexión para decidir s01/s02
        struct sockaddr_in local;
        socklen_t llen = sizeof(local);
        if (getsockname(c, (struct sockaddr*)&local, &llen) < 0) { perror("getsockname"); close(c); continue; }

        // Convierto la IP local a string legible 
        char local_ip[INET_ADDRSTRLEN] = "";
        inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip));

        // Decido la carpeta destino según el alias local
        const char *bucket = "s01";              // por defecto s01
        if (strcmp(local_ip, S02_IP) == 0) bucket = "s02";
        else if (strcmp(local_ip, S01_IP) == 0) bucket = "s01";

        // Armo un timestamp para el nombre del archivo: YYYYmmdd_HHMMSS
        time_t t = time(NULL);
        struct tm *ptm = localtime(&t);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", ptm);

        char path[512];
        snprintf(path, sizeof(path), "%s/%s/recv_%s.dat", home, bucket, ts);

        // Abro el archivo en binario para ir volcando lo que llegue por el socket
        FILE *out = fopen(path, "wb");
        if (!out) { perror("fopen"); close(c); continue; }

        // Leo del socket en bloques y escribo al archivo hasta que el cliente cierre (recv devuelve 0)
        char buf[BUF];
        ssize_t n;
        size_t total = 0;
        while ((n = recv(c, buf, sizeof(buf), 0)) > 0) {
            fwrite(buf, 1, (size_t)n, out);
            total += (size_t)n;
        }
        fclose(out);   // cierro el archivo
        close(c);      // cierro el socket con el cliente

        // Mensaje de confirmación en consola para saber donde se guardo
        printf("[SERVER] %zu bytes guardados en %s (alias local %s → carpeta %s)\n",
               total, path, local_ip, bucket);
    }

    close(ls);
    return 0;
}
