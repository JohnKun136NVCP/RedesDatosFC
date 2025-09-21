// clientMulti.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFSZ 4096

static const int VALID_PORTS[] = {49200, 49201, 49202};
static const int N_VALID = (int)(sizeof(VALID_PORTS)/sizeof(VALID_PORTS[0]));


static char* read_file(const char* path, size_t* out_len) {
    FILE* fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return NULL; }
    char *data = NULL;
    size_t cap = 0, len = 0;
    char buf[BUFSZ];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (len + n + 1 > cap) {
            cap = (cap ? cap*2 : 8192);
            while (cap < len + n + 1) cap *= 2;
            char *tmp = (char*)realloc(data, cap);
            if (!tmp) { perror("realloc"); free(data); fclose(fp); return NULL; }
            data = tmp;
        }
        memcpy(data + len, buf, n);
        len += n;
    }
    if (ferror(fp)) { perror("fread"); free(data); fclose(fp); return NULL; }
    fclose(fp);
    if (!data) { data = (char*)malloc(1); if (!data) return NULL; data[0] = '\0'; }
    data[len] = '\0';
    if (out_len) *out_len = len;
    return data;
}

static int talk_one(const char* ip, int listen_port, int target_port, int k,
                    const char* body, size_t body_len, const char* fname) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(listen_port);
    sa.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        fprintf(stderr, "[CLIENT]  !!!! No pude conectar a %s:%d (%s)\n",
                ip, listen_port, strerror(errno));
        close(sock);
        return -1;
    }

    printf("\n[CLIENT] ----> Conectado a %s:%d (target=%d, k=%d, archivo=%s, %zu bytes)\n",
           ip, listen_port, target_port, k, fname, body_len);

    char header[96];
    int hlen = snprintf(header, sizeof(header), "%d %d %zu\n", target_port, k, body_len);
    if (send(sock, header, hlen, 0) < 0) perror("send header");

    size_t sent = 0;
    while (sent < body_len) {
        size_t chunk = body_len - sent;
        if (chunk > BUFSZ) chunk = BUFSZ;
        int n = send(sock, body + sent, (int)chunk, 0);
        if (n <= 0) { perror("send body"); break; }
        sent += (size_t)n;
    }

    shutdown(sock, SHUT_WR); 

    // Respuesta
    char resp[BUFSZ + 1];
    int r;
    while ((r = recv(sock, resp, BUFSZ, 0)) > 0) {
        resp[r] = '\0';
        fputs(resp, stdout);
    }
    if (r < 0) perror("recv");

    close(sock);
    return 0;
}

typedef struct {
    int target_port;
    int k;
    char **files;
    int   nfiles;
} group_t;

static void free_group(group_t *g){
    if (!g) return;
    for (int i=0;i<g->nfiles;i++) free(g->files[i]);
    free(g->files);
}

static int parse_group(const char* spec, group_t* out){
    char *work = strdup(spec);
    if (!work) return -1;

    // formato --> puerto:desplazamiento:archivos
    char *save = NULL;
    char *tok_port = strtok_r(work, ":", &save);
    char *tok_k    = tok_port ? strtok_r(NULL, ":", &save) : NULL;
    char *tok_csv  = tok_k    ? strtok_r(NULL, ":", &save) : NULL;

    if (!tok_port || !tok_k || !tok_csv) {
        fprintf(stderr, "Formato inválido: '%s' (esperado: PUERTO:K:archivo1[,archivo2,...])\n", spec);
        free(work);
        return -1;
    }

    out->target_port = atoi(tok_port);
    out->k           = atoi(tok_k);
    out->files       = NULL;
    out->nfiles      = 0;

    char *csv_copy = strdup(tok_csv);
    if (!csv_copy) { free(work); return -1; }

    char *save2 = NULL;
    for (char *t = strtok_r(csv_copy, ",", &save2); t; t = strtok_r(NULL, ",", &save2)) {

        while (*t==' '||*t=='\t') t++;
        size_t L = strlen(t);
        while (L>0 && (t[L-1]==' '||t[L-1]=='\t'||t[L-1]=='\n'||t[L-1]=='\r')) t[--L]='\0';

        char *fname = strdup(t);
        if (!fname) { free(csv_copy); free(work); return -1; }

        char **tmp = (char**)realloc(out->files, (out->nfiles+1)*sizeof(char*));
        if (!tmp) { free(fname); free(csv_copy); free(work); return -1; }
        out->files = tmp;
        out->files[out->nfiles++] = fname;
    }

    free(csv_copy);
    free(work);
    if (out->nfiles == 0) {
        fprintf(stderr, "Grupo '%s' no contiene archivos.\n", spec);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr,
          "Uso: %s <IP_SERVIDOR> <PUERTO:K:archivo1[,archivo2[,...]]> [ ... más grupos ... ]\n"
          "Ejemplo:\n"
          "  %s 192.168.1.14 49200:5:mensaje.txt 49202:3:uno.txt,dos.txt 49201:7:tres.txt\n",
          argv[0], argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];

    for (int a = 2; a < argc; ++a) {
        group_t g;
        if (parse_group(argv[a], &g) != 0) {
            fprintf(stderr, "Saltando grupo inválido: %s\n", argv[a]);
            continue;
        }

        printf("\n========== Grupo: target=%d, k=%d ==========\n", g.target_port, g.k);
        for (int f = 0; f < g.nfiles; ++f) {
            size_t body_len = 0;
            char *body = read_file(g.files[f], &body_len);
            if (!body) {
                fprintf(stderr, "[CLIENT]  !!!! No pude leer '%s'\n", g.files[f]);
                continue;
            }

            // Conectarse a cada servidor y enviar el mismo paquete
            for (int i = 0; i < N_VALID; ++i) {
                (void)talk_one(server_ip, VALID_PORTS[i], g.target_port, g.k,
                               body, body_len, g.files[f]);
            }

            free(body);
        }
        free_group(&g);
    }

    return 0;
}

