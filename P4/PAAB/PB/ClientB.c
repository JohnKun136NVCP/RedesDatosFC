#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF 1024
#define DEFAULT_PORT 49200

static int read_line(int fd, char *buf, int max){
    int i=0; while(i<max-1){ char c; int r=recv(fd,&c,1,0); if(r<=0) return r; buf[i++]=c; if(c=='\n') break; }
    buf[i]=0; return i;
}

static int read_n(int fd, char *buf, int n){
    int g=0; while(g<n){ int r=recv(fd,buf+g,n-g,0); if(r<=0) return r; g+=r; } return g;
}

// estado con timestamp a status.log y también a stdout.
static void log_status(const char *state){
    FILE *fp=fopen("status.log","a"); time_t t=time(NULL); struct tm *tm=localtime(&t);
    char ts[64]; strftime(ts,sizeof ts,"%Y-%m-%d %H:%M:%S",tm);
    if(fp){ fprintf(fp,"%s | %s\n",ts,state); fclose(fp); }
    printf("%s | %s\n",ts,state);
}

// Resuelve alias o IP literal
static int resolve(const char *host, struct in_addr *out){
    struct hostent *he=gethostbyname(host);
    if(he && he->h_addr_list && he->h_addr_list[0]){ memcpy(out, he->h_addr_list[0], he->h_length); return 0; }
    return inet_pton(AF_INET, host, out)==1 ? 0 : -1;
}

// Envía un archivo a un servidor dado usando handshake de puerto.
static int send_one(const char *server, int base_port, const char *path){
    // Carga archivo completo en memoria.
    FILE *f=fopen(path,"rb");
    if(!f){ perror("fopen"); return -1; }
    fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
    if(len<=0){ fprintf(stderr,"Archivo vacío\n"); fclose(f); return -1; }
    char *body=(char*)malloc(len); if(!body){ fprintf(stderr,"sin memoria\n"); fclose(f); return -1; }
    if(fread(body,1,len,f)!=(size_t)len){ perror("fread"); free(body); fclose(f); return -1; }
    fclose(f);

    // Conecta a puerto base y solicita puerto de trabajo.
    int s0=socket(AF_INET,SOCK_STREAM,0); if(s0<0){ perror("socket"); free(body); return -1; }
    struct sockaddr_in a0; memset(&a0,0,sizeof a0); a0.sin_family=AF_INET; a0.sin_port=htons(base_port);
    if(resolve(server,&a0.sin_addr)!=0){ perror("resolver"); close(s0); free(body); return -1; }
    if(connect(s0,(struct sockaddr*)&a0,sizeof a0)<0){ perror("connect base"); close(s0); free(body); return -1; }
    log_status("conectado a PUERTO_BASE; solicitando puerto asignado");

    // Lee "PORT:<n>\n\n".
    char line[BUF]; if(read_line(s0,line,sizeof line)<=0){ fprintf(stderr,"sin respuesta\n"); close(s0); free(body); return -1; }
    int newport=0; sscanf(line,"PORT:%d",&newport); read_line(s0,line,sizeof line); close(s0);
    if(newport<=0){ fprintf(stderr,"puerto asignado inválido\n"); free(body); return -1; }
    char msg[128]; snprintf(msg,sizeof msg,"puerto asignado = %d",newport); log_status(msg);

    // Reconecta a puerto asignado.
    int s=socket(AF_INET,SOCK_STREAM,0); if(s<0){ perror("socket"); free(body); return -1; }
    a0.sin_port=htons(newport);
    if(connect(s,(struct sockaddr*)&a0,sizeof a0)<0){ perror("connect asignado"); close(s); free(body); return -1; }
    log_status("conectado a puerto asignado; enviando archivo");

    // Envía cabeceras y cuerpo.
    const char *fname=strrchr(path,'/'); fname=fname?fname+1:path;
    char hdr[256]; int n=snprintf(hdr,sizeof hdr,"LEN:%ld\nNAME:%s\n\n",len,fname);
    if(send(s,hdr,n,0)!=n){ perror("send hdr"); close(s); free(body); return -1; }

    long sent=0; while(sent<len){ ssize_t w=send(s,body+sent,len-sent,0); if(w<=0){ perror("send body"); close(s); free(body); return -1; } sent+=w; }

    // Lee confirmación.
    if(read_line(s,line,sizeof line)>0){
        if(!strncmp(line,"OK",2)) log_status("transmitido OK");
        else log_status("error en servidor");
    }else log_status("sin respuesta del servidor");

    close(s); free(body); return 0;
}

// Dada la propia etiqueta, devuelve en 'out'.
static void peers_for(const char *self, const char **out, int *k){
    static const char *all[4]={"s01","s02","s03","s04"};
    int j=0; for(int i=0;i<4;i++){ if(strcmp(self,all[i])!=0) out[j++]=all[i]; }
    *k=j;
}

int main(int argc, char **argv){
    // Modos:
    // 1) ./client <server> <port> <file>
    // 2) ./client <server> <file>            (usa DEFAULT_PORT)
    // 3) ./client --fanout s0X <file>        (envía a todos menos s0X)
    if(argc==4 && strcmp(argv[1],"--fanout")!=0){
        return send_one(argv[1], atoi(argv[2]), argv[3])==0 ? 0 : 1;
    } else if(argc==3 && strcmp(argv[1],"--fanout")!=0){
        return send_one(argv[1], DEFAULT_PORT, argv[2])==0 ? 0 : 1;
    } else if(argc==4 && strcmp(argv[1],"--fanout")==0){
        const char *self=argv[2], *file=argv[3];
        const char *dst[4]; int k=0; peers_for(self,dst,&k);
        int rc=0; for(int i=0;i<k;i++){ printf("==> %s\n", dst[i]); if(send_one(dst[i], DEFAULT_PORT, file)!=0) rc=1; }
        return rc;
    } else {
        fprintf(stderr,
            "Uso:\n"
            "  %s <alias|IP> <PUERTO_BASE> <ARCHIVO>\n"
            "  %s <alias|IP> <ARCHIVO>            (puerto %d)\n"
            "  %s --fanout s01|s02|s03|s04 <ARCHIVO>\n",
            argv[0], argv[0], DEFAULT_PORT, argv[0]);
        return 1;
    }
}
