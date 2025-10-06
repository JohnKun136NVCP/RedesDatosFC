#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 49200
#define BUFFER_SIZE 1024
#define MAX_LISTEN 32 // máximo de sockets en escucha simultánea

// Devuelve bytes leídos o <=0 en error/cierre.
static int read_line(int fd, char *buf, int max){
    int i=0; while(i<max-1){ char c; int r=recv(fd,&c,1,0); if(r<=0) return r; buf[i++]=c; if(c=='\n') break; }
    buf[i]=0; return i;
}

// Devuelve 'n' o <=0 en error/cierre.
static int read_n(int fd, char *buf, int n){
    int g=0; while(g<n){ int r=recv(fd,buf+g,n-g,0); if(r<=0) return r; g+=r; } return g;
}

// Abre un socket de trabajo en el primer puerto libre.
// Devuelve fd de escucha y escribe el puerto en 'out_port', o -1 en error.
static int open_worker(int base, int *out_port){
    for(int p=base+1;p<=base+200;p++){
        int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return -1;
        int opt=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
        struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY; a.sin_port=htons(p);
        if(bind(fd,(struct sockaddr*)&a,sizeof a)==0 && listen(fd,8)==0){ *out_port=p; return fd; }
        close(fd);
    } return -1;
}

// Resuelve el alias
static int resolve_alias_ip(const char *alias, in_addr_t *out){
    struct hostent *h=gethostbyname(alias);
    if(!h || !h->h_addr_list || !h->h_addr_list[0]) return -1;
    *out = *(in_addr_t*)h->h_addr_list[0]; return 0;
}

// Selecciona directorio de destino según la IP local del socket.
// Usa el último octeto
static const char* pick_dir_from_local(int sock){
    static char path[256];
    struct sockaddr_in local; socklen_t slen=sizeof local;
    if(getsockname(sock,(struct sockaddr*)&local,&slen)==0){
        unsigned last = (unsigned)(ntohl(local.sin_addr.s_addr)&0xFF);
        const char *home=getenv("HOME");
        if(last==101) { snprintf(path,sizeof path,"%s/s01",home); return path; }
        if(last==102) { snprintf(path,sizeof path,"%s/s02",home); return path; }
        if(last==103) { snprintf(path,sizeof path,"%s/s03",home); return path; }
        if(last==104) { snprintf(path,sizeof path,"%s/s04",home); return path; }
    }
    snprintf(path,sizeof path,"%s/s01",getenv("HOME")); return path;
}

int main(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"uso: %s s01 [s02 ...]\n", argv[0]); return 1; }

    // Garantiza existencia por si acaso.
    char d[256]; const char *home=getenv("HOME");
    for(int i=1;i<=4;i++){ snprintf(d,sizeof d,"%s/s0%d",home,i); mkdir(d,0755); }

    int listen_fds[MAX_LISTEN]; int nlisten=0;

    // Crea un socket en escucha por cada alias.
    for(int i=1;i<argc && nlisten<MAX_LISTEN;i++){
        in_addr_t ip; if(resolve_alias_ip(argv[i], &ip)!=0){ fprintf(stderr,"alias no resuelto: %s\n", argv[i]); continue; }
        int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0){ perror("socket"); return 1; }
        int opt=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
        struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(PORT); a.sin_addr.s_addr=ip;
        if(bind(fd,(struct sockaddr*)&a,sizeof a)<0){ perror("bind"); close(fd); continue; }
        if(listen(fd,16)<0){ perror("listen"); close(fd); continue; }
        listen_fds[nlisten++]=fd;
        char ipstr[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&ip,ipstr,sizeof ipstr);
        printf("[LISTEN] %s:%d (%s)\n", ipstr, PORT, argv[i]);
    }
    if(nlisten==0){ fprintf(stderr,"ningún alias válido\n"); return 1; }

    // multiplexa aceptaciones con select() en todos los sockets.
    while(1){
        fd_set rfds; FD_ZERO(&rfds); int mx=-1;
        for(int i=0;i<nlisten;i++){ FD_SET(listen_fds[i], &rfds); if(listen_fds[i]>mx) mx=listen_fds[i]; }
        if(select(mx+1, &rfds, NULL, NULL, NULL)<=0) continue;

        for(int i=0;i<nlisten;i++){
            if(!FD_ISSET(listen_fds[i], &rfds)) continue;

            // asigna puerto de trabajo y cierra.
            struct sockaddr_in caddr; socklen_t alen=sizeof caddr;
            int c0 = accept(listen_fds[i], (struct sockaddr*)&caddr, &alen);
            if(c0<0){ perror("accept"); continue; }

            int wport=0; int wlisten=open_worker(PORT,&wport);
            if(wlisten<0){ const char *err="PORT:0\n\n"; send(c0,err,strlen(err),0); close(c0); continue; }
            char line[64]; int n=snprintf(line,sizeof line,"PORT:%d\n\n", wport);
            send(c0,line,n,0); close(c0);

            // segunda conexión del cliente al puerto de trabajo.
            struct sockaddr_in cc; socklen_t clen=sizeof cc;
            int cs=accept(wlisten,(struct sockaddr*)&cc,&clen);
            if(cs<0){ perror("accept worker"); close(wlisten); continue; }

            // recibe encabezados LEN/NAME y el cuerpo del archivo.
            int len=0; char name[BUFFER_SIZE]={0};
            if(read_line(cs,line,sizeof line)<=0){ close(cs); close(wlisten); continue; }
            sscanf(line,"LEN:%d",&len);
            if(read_line(cs,line,sizeof line)<=0){ close(cs); close(wlisten); continue; }
            if(!strncmp(line,"NAME:",5)){
                strncpy(name,line+5,sizeof(name)-1);
                size_t L=strlen(name); if(L && name[L-1]=='\n') name[L-1]=0;
            }
            read_line(cs,line,sizeof line); 

            if(len<=0 || len>(1<<22)){ const char *rej="ERR\n"; send(cs,rej,strlen(rej),0); close(cs); close(wlisten); continue; }

            char *body=(char*)malloc(len); if(!body){ const char *rej="ERR\n"; send(cs,rej,strlen(rej),0); close(cs); close(wlisten); continue; }
            if(read_n(cs,body,len)!=len){ free(body); close(cs); close(wlisten); continue; }

            // decide carpeta según IP local y guarda el archivo.
            const char *dir = pick_dir_from_local(cs);
            char out[512]; snprintf(out,sizeof out,"%s/%s", dir, name[0]?name:"recv.txt");
            FILE *fo=fopen(out,"wb"); if(!fo){ const char *rej="ERR\n"; send(cs,rej,strlen(rej),0); free(body); close(cs); close(wlisten); continue; }
            fwrite(body,1,len,fo); fclose(fo); free(body);

            const char *ok="OK\n"; send(cs,ok,strlen(ok),0);
            printf("[SAVE] %s\n", out);

            close(cs); close(wlisten);
        }
    }
}

