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

// Alias de la Parte B
#define S01_IP "192.168.0.101"
#define S02_IP "192.168.0.102"
#define S03_IP "192.168.0.103"
#define S04_IP "192.168.0.104"

// Crea el directorio si no existe (para s01, s02, s03, s04)
static void ensure_dir(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
    }
}

int main(int argc, char *argv[]) {
    // Valido argumento: necesito 1 puerto (ej. 49200)
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\nEj.: %s 49200\n", argv[0], argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    //  Socket de escucha en todas las IP locales (INADDR_ANY) 
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { perror("socket"); return 1; }

    // Reusar puerto si queda en TIME_WAIT
    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Estructura de dirección/puerto del servidor
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)port);
    addr.sin_addr.s_addr = INADDR_ANY;   // escucha en s01..s04 también

    // Enlazo y pongo a escuchar
    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(ls); return 1; }
    if (listen(ls, 10) < 0)                                  { perror("listen"); close(ls); return 1; }

    printf("[SERVER] escuchando en puerto %d ...\n", port);

    //  Preparo rutas ~/s01..~/s04 
    const char *home = getenv("HOME"); if (!home) home = ".";
    char d1[256], d2[256], d3[256], d4[256];
    snprintf(d1, sizeof d1, "%s/s01", home); ensure_dir(d1);
    snprintf(d2, sizeof d2, "%s/s02", home); ensure_dir(d2);
    snprintf(d3, sizeof d3, "%s/s03", home); ensure_dir(d3);
    snprintf(d4, sizeof d4, "%s/s04", home); ensure_dir(d4);

    //  acepto conexiones y guardo lo recibido 
    for (;;) {
        // Acepto al cliente
        struct sockaddr_in cli; socklen_t clen = sizeof(cli);
        int c = accept(ls, (struct sockaddr*)&cli, &clen);
        if (c < 0) { perror("accept"); continue; }

        // Obtengo la IP LOCAL donde cayó la conexión (para saber el alias s01..s04)
        struct sockaddr_in local; socklen_t llen = sizeof(local);
        if (getsockname(c, (struct sockaddr*)&local, &llen) < 0) {
            perror("getsockname"); close(c); continue;
        }

        // Convierto la IP local a string (ej. "192.168.0.102")
        char local_ip[INET_ADDRSTRLEN] = "";
        inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip));

        // Decido la carpeta destino según el alias local
        const char *bucket = "s01";                // por defecto s01
        if      (strcmp(local_ip, S02_IP) == 0) bucket = "s02";
        else if (strcmp(local_ip, S03_IP) == 0) bucket = "s03";
        else if (strcmp(local_ip, S04_IP) == 0) bucket = "s04";
        else if (strcmp(local_ip, S01_IP) == 0) bucket = "s01";

        // Armo nombre del archivo con timestamp: recv_YYYYmmdd_HHMMSS.dat
        time_t t = time(NULL);
        struct tm *ptm = localtime(&t);
        char ts[32]; strftime(ts, sizeof ts, "%Y%m%d_%H%M%S", ptm);

        // Ruta final de guardado: ~/s0X/recv_timestamp.dat
        char path[512];
        snprintf(path, sizeof path, "%s/%s/recv_%s.dat", home, bucket, ts);

        // Abro archivo de salida y vuelco todo lo que llegue por el socket
        FILE *out = fopen(path, "wb");
        if (!out) { perror("fopen"); close(c); continue; }

        char buf[BUF]; ssize_t n; size_t total = 0;
        // Bucle de recepción: leo del socket y escribo al archivo
        while ((n = recv(c, buf, sizeof buf, 0)) > 0) {
            fwrite(buf, 1, (size_t)n, out);
            total += (size_t)n; // contador de bytes
        }
        fclose(out);
        close(c);

        // Mensaje de confirmación con bytes, ruta y alias
        printf("[SERVER] %zu bytes guardados en %s (alias local %s -> %s)\n",
               total, path, local_ip, bucket);
    }

    close(ls);
    return 0;
}

