#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

void ensure_inbox() {
    struct stat st;
    if (stat("inbox", &st) == -1)
        mkdir("inbox", 0777);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind");
        return 1;
    }

    listen(s, 5);
    ensure_inbox();

    printf("[SERVIDOR] Escuchando en puerto %d...\n", port);

    while (1) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int cs = accept(s, (struct sockaddr*)&cli, &cl);
        if (cs < 0) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("\n[SERVIDOR] Conexión recibida de %s\n", client_ip);
        printf("[SERVIDOR] Esperando turno...\n");

        char filename[128];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        snprintf(filename, sizeof(filename),
                 "inbox/archivo_%04d%02d%02d_%02d%02d%02d.bin",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);

        FILE *f = fopen(filename, "wb");
        if (!f) { perror("fopen"); close(cs); continue; }

        char buf[4096];
        ssize_t n;
        while ((n = recv(cs, buf, sizeof(buf), 0)) > 0)
            fwrite(buf, 1, n, f);

        fclose(f);
        close(cs);

        printf("[SERVIDOR] Archivo recibido y guardado como %s\n", filename);
        printf("[SERVIDOR] Turno completado. Esperando nueva conexión...\n");
    }

    return 0;
}
