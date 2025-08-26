/* P2 | Cliente TCP | Clave: wpvkpi | IGG */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 7006
#define BUFSZ 1024

static int read_line(int fd, char *buf, size_t max){
    size_t i=0;
    while (i+1<max){
        char c; ssize_t r = recv(fd, &c, 1, 0);
        if(r <= 0) return -1;
        if(c == '\n'){ buf[i]=0; return (int)i; }
        buf[i++] = c;
    }
    buf[i]=0; return (int)i;
}

int main(int argc, char *argv[]){
    if(argc != 3){
        fprintf(stderr,"Uso: %s <clave_en_claro> <shift>\n", argv[0]);
        fprintf(stderr,"Ej:  %s wpvkpi 28\n", argv[0]);
        return 1;
    }
    const char *key_plain = argv[1];
    int shift = atoi(argv[2]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){ perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    /* mismo host (prueba local); si tu server corre en otra VM, pon su IP */
    srv.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(s, (struct sockaddr*)&srv, sizeof(srv)) < 0){
        perror("connect"); close(s); return 1;
    }

    char msg[BUFSZ];
    snprintf(msg, sizeof(msg), "%s %d\n", key_plain, shift);
    if(send(s, msg, strlen(msg), 0) <= 0){
        perror("send"); close(s); return 1;
    }

    char line[BUFSZ];
    if(read_line(s, line, sizeof(line)) < 0){
        fprintf(stderr,"Error leyendo respuesta del servidor.\n");
        close(s); return 1;
    }
    printf("[Servidor] %s\n", line);

    if(strstr(line, "ACCESS GRANTED")){
        FILE *out = fopen("info.txt","wb");
        if(!out){ perror("fopen info.txt"); close(s); return 1; }
        char buf[4096];
        ssize_t r;
        while ((r = recv(s, buf, sizeof(buf), 0)) > 0){
            fwrite(buf, 1, r, out);
        }
        fclose(out);
        printf("[Cliente] Archivo recibido y guardado como info.txt\n");
    }

    close(s);
    return 0;
}
