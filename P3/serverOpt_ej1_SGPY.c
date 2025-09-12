// server.c - escucha en el puerto dado y procesa si coincide con target_port
// gcc server.c -o server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static void caesar(char *s, int shift){
    for(char *p = s; *p; ++p){
        if(isalpha((unsigned char)*p)){
            char base = islower((unsigned char)*p) ? 'a' : 'A';
            *p = (char)(base + (((*p - base) + shift) % 26 + 26) % 26);
        }
    }
}

static int recv_all(int fd, void *buf, size_t len){
    char *p = (char*)buf; size_t rcv = 0;
    while(rcv < len){
        ssize_t n = recv(fd, p + rcv, len - rcv, 0);
        if(n == 0) return 0;          // peer closed
        if(n < 0) { if(errno == EINTR) continue; return -1; }
        rcv += (size_t)n;
    }
    return 1;
}

static int send_all(int fd, const void *buf, size_t len){
    const char *p = (const char*)buf; size_t snd = 0;
    while(snd < len){
        ssize_t n = send(fd, p + snd, len - snd, 0);
        if(n <= 0){ if(errno == EINTR) continue; return -1; }
        snd += (size_t)n;
    }
    return 1;
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }
    int listen_port = atoi(argv[1]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){ perror("socket"); return 1; }

    int opt = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons((uint16_t)listen_port);

    if(bind(s, (struct sockaddr*)&srv, sizeof(srv)) < 0){ perror("bind"); return 1; }
    if(listen(s, 16) < 0){ perror("listen"); return 1; }

    printf("[Server %d] listening...\n", listen_port);

    for(;;){
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int c = accept(s, (struct sockaddr*)&cli, &clilen);
        if(c < 0){ if(errno==EINTR) continue; perror("accept"); break; }

        // --- recibe cabecera ---
        uint32_t net_target, net_shift, net_len;
        if(!recv_all(c, &net_target, sizeof(net_target)) ||
           !recv_all(c, &net_shift,  sizeof(net_shift))  ||
           !recv_all(c, &net_len,    sizeof(net_len)) ){
            close(c); continue;
        }
        int target_port = (int)ntohl(net_target);
        int shift       = (int)ntohl(net_shift);
        int content_len = (int)ntohl(net_len);

        // --- recibe contenido ---
        char *buf = (char*)calloc((size_t)content_len + 1, 1);
        if(!buf){ close(c); continue; }
        if(!recv_all(c, buf, (size_t)content_len)){ free(buf); close(c); continue; }

        if(target_port == listen_port){
            // procesa (César) y responde status=1 + texto
            caesar(buf, shift);
            uint32_t status = htonl(1);
            uint32_t plen   = htonl((uint32_t)content_len);
            if(send_all(c, &status, sizeof(status)) &&
               send_all(c, &plen,   sizeof(plen))   &&
               send_all(c, buf,     (size_t)content_len)){
                // ok
            }
            printf("[Server %d] PROCESADO (shift=%d)\n", listen_port, shift);
        }else{
            // rechaza: status=0
            uint32_t status = htonl(0);
            send_all(c, &status, sizeof(status));
            printf("[Server %d] RECHAZADO (target=%d)\n", listen_port, target_port);
        }

        free(buf);
        close(c);
    }

    close(s);
    return 0;
}
