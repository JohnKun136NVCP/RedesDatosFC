// serverOpt.c
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#define BUFSZ 4096

#define C_RESET "\x1b[0m"
#define C_RED   "\x1b[31m"
#define C_GRN   "\x1b[32m"
#define C_CYN   "\x1b[36m"

static void caesar_inplace(char *s, int k){
    if (k < 0) k = (26 - ((-k)%26)) % 26; else k %= 26;
    for (char *p = s; *p; ++p){
        if ('a' <= *p && *p <= 'z') *p = 'a' + ((*p - 'a' + k) % 26);
        else if ('A' <= *p && *p <= 'Z') *p = 'A' + ((*p - 'A' + k) % 26);
    }
}

static void set_nonblock(int fd){
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) fl = 0;
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static ssize_t read_full(int fd, char *buf, size_t n){
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r == 0) return 0;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)r;
    }
    return (ssize_t)got;
}

static void handle_client(int cfd, int myport){

    char acc[BUFSZ]; size_t used = 0;
    int target = -1, k = 0;
    size_t need = 0;

    for(;;){
        ssize_t n = recv(cfd, acc + used, sizeof(acc) - used - 1, 0);
        if (n <= 0) { close(cfd); return; }
        used += (size_t)n; acc[used] = '\0';

        char *nl = memchr(acc, '\n', used);
        if (nl){
            size_t header_len = (size_t)(nl - acc);
            char header[256];
            if (header_len >= sizeof(header)) header_len = sizeof(header) - 1;
            memcpy(header, acc, header_len);
            header[header_len] = '\0';

            if (sscanf(header, "%d %d %zu", &target, &k, &need) != 3){
                const char *bad = "BAD REQUEST\n";
                send(cfd, bad, strlen(bad), 0);
                close(cfd);
                return;
            }

            size_t rest = used - (header_len + 1);
            char *body = (char*)malloc(need + 1);
            size_t blen = 0;
            if (rest > 0) {
                size_t take = (rest > need ? need : rest);
                memcpy(body, nl + 1, take);
                blen = take;
            }

            if (blen < need) {
                ssize_t r = read_full(cfd, body + blen, need - blen);
                if (r <= 0) { free(body); close(cfd); return; }
                blen += (size_t)r;
            }
            body[blen] = '\0';

            if (target == myport){
                caesar_inplace(body, k);
                const char *ok = "PROCESADO\n";
                send(cfd, ok, strlen(ok), 0);
                if (blen) send(cfd, body, blen, 0);

                printf(C_GRN "[*][SERVER %d] Request accepted..." C_RESET "\n", myport);
                printf(C_GRN "[*][SERVER %d] File received and encrypted:" C_RESET "\n", myport);
                if (blen) {
                    fwrite(body, 1, blen, stdout);
                    if (body[blen-1] != '\n') putchar('\n');
                } else {
                    printf("(empty)\n");
                }
                fflush(stdout);
            } else {
                const char *no = "RECHAZADO\n";
                send(cfd, no, strlen(no), 0);
                printf(C_RED "[*][SERVER %d] Request rejected (client requested port %d)"
                       C_RESET "\n", myport, target);
                fflush(stdout);
            }

            free(body);
            close(cfd);
            return;
        }

        if (used >= sizeof(acc) - 1) {
            const char *bad = "BAD HEADER\n";
            send(cfd, bad, strlen(bad), 0);
            close(cfd);
            return;
        }
    }
}

static int make_listener(int port){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return -1; }
    if (listen(s, 16) < 0) { perror("listen"); close(s); return -1; }
    set_nonblock(s);
    printf(C_CYN "[*][CLIENT %d] LISTENING..." C_RESET "\n", port);
    fflush(stdout);
    return s;
}

int main(void){
    const int ports[] = {49200, 49201, 49202};
    const int N = (int)(sizeof(ports)/sizeof(ports[0]));
    int ls[N];

    // Crear 3 sockets de escucha
    for (int i=0;i<N;i++){
        ls[i] = make_listener(ports[i]);
        if (ls[i] < 0){
            // Cerrar los que sí se abrieron antes de salir
            for (int j=0;j<i;j++) close(ls[j]);
            return 1;
        }
    }

    for(;;){
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        for (int i=0;i<N;i++){
            FD_SET(ls[i], &rfds);
            if (ls[i] > maxfd) maxfd = ls[i];
        }

        // Esperamos actividad en cualquiera de los 3 sockets
        int ready = select(maxfd+1, &rfds, NULL, NULL, NULL);
        if (ready < 0){
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // Revisamos cualess sockets estan listos para aceptar
        for (int i=0;i<N;i++){
            if (FD_ISSET(ls[i], &rfds)){
                for(;;){
                    int cfd = accept(ls[i], NULL, NULL);
                    if (cfd < 0){
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        break;
                    }
                    // Atender la conexión completa (lectura, decisión, respuesta)
                    handle_client(cfd, ports[i]);
                }
            }
        }
    }

    for (int i=0;i<N;i++) close(ls[i]);
    return 0;
}

