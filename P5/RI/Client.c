#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Conecta a host:port (IPv4, TCP). Retorna fd o -1
static int dial(const char* host,const char* port){
    struct addrinfo h,*it=0; memset(&h,0,sizeof(h));
    h.ai_family=AF_INET;            // IPv4
    h.ai_socktype=SOCK_STREAM;      // TCP

    if(getaddrinfo(host,port,&h,&it)!=0) return -1;

    int fd=-1;
    for(struct addrinfo*p=it;p;p=p->ai_next){
        fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if(fd<0) continue;
        if(connect(fd,p->ai_addr,p->ai_addrlen)==0){
            freeaddrinfo(it);
            return fd;              // éxito
        }
        close(fd); fd=-1;           // intenta siguiente dirección
    }
    freeaddrinfo(it);
    return -1;
}


static int rdline(int fd,char *b,size_t m){
    size_t i=0; char c;
    while(i+1<m){
        ssize_t r=recv(fd,&c,1,0);
        if(r<=0) return -1;         // error o cierre
        if(c=='\n'){ b[i]=0; return (int)i; }
        b[i++]=c;
    }
    b[i]=0;
    return (int)i;                  // línea truncada si excede m-1
}

// Fija timeouts de recv/send en milisegundos. Evita bloqueos largos.
static void set_timeouts(int fd,int rcv_ms,int snd_ms){
    struct timeval tr={rcv_ms/1000,(rcv_ms%1000)*1000};
    struct timeval ts={snd_ms/1000,(snd_ms%1000)*1000};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tr,sizeof(tr));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&ts,sizeof(ts));
}

int main(int argc,char **argv){
    if(argc<5){
        fprintf(stderr,"Uso:\n  %s <host> <PORT_BASE> status <log>\n  %s <host> <PORT_BASE> put <archivo>\n",argv[0],argv[0]);
        return 1;
    }
    const char* host = argv[1];
    const char* pbase= argv[2];
    const char* mode = argv[3];
    const char* arg  = argv[4];

    //  Conexión base: el server devuelve PORT <efimero>\n
    int c0=dial(host,pbase);
    if(c0<0){ perror("connect base"); return 1; }

    char line[256];
    if(rdline(c0,line,sizeof(line))<0){
        fprintf(stderr,"handshake\n");
        close(c0);
        return 1;
    }
    close(c0);

    //  Parse del puerto efímero y reconexión
    unsigned eport=0;
    if(sscanf(line,"PORT %u",&eport)!=1){
        fprintf(stderr,"PROTO: %s\n",line);
        return 1;
    }
    char sport[16]; snprintf(sport,sizeof(sport),"%u",eport);

    int c1=dial(host,sport);
    if(c1<0){ perror("connect eph"); return 1; }
    set_timeouts(c1,5000,5000);     // recv/send

    // Modo STATUS: pide estado y lo loguea con timestamp
    if(strcmp(mode,"status")==0){
        const char*q="STATUS\n";
        send(c1,q,strlen(q),0);

        if(rdline(c1,line,sizeof(line))<0){
            close(c1);
            return 1;
        }
        close(c1);

        time_t t=time(NULL);
        struct tm *lt=localtime(&t);
        char ts[32]; strftime(ts,sizeof(ts),"%F %T",lt);

        FILE *f=fopen(arg,"a");
        if(!f){ perror("log"); return 1; }
        fprintf(f,"%s %s\n",ts,line);
        fclose(f);

        printf("[CLIENTE] %s %s\n",ts,line);
        return 0;
    }

    // Modo PUT: envía archivo respetando GO/WAIT 
    if(strcmp(mode,"put")==0){
        // Abrir archivo y obtener tamaño real a enviar
        const char* fname=arg;
        FILE *fp=fopen(fname,"rb");
        if(!fp){ perror("archivo"); close(c1); return 1; }

        struct stat st;
        if(stat(fname,&st)<0){ perror("stat"); fclose(fp); close(c1); return 1; }

        // Solo el nombre base, sin ruta
        const char *bn=strrchr(fname,'/');
        bn = bn ? bn+1 : fname;

        //  Enviar encabezados: nombre y tamaño 
        char hdr[512];
        snprintf(hdr,sizeof(hdr),"PUT %s\n%lld\n",bn,(long long)st.st_size);
        if(send(c1,hdr,strlen(hdr),0)<0){
            perror("send hdr"); fclose(fp); close(c1); return 1;
        }

        //  Handshake: esperar GO o WAIT
        //    - Si WAIT: no es turno. Salir limpio.
        //    - Si GO: enviar cuerpo.
       
        char resp[64];
        set_timeouts(c1,300,5000);  // espera corta para línea GO/WAIT
        int r = rdline(c1,resp,sizeof(resp));
        if(r>0){
            if(strcmp(resp,"WAIT")==0){
                printf("[CLIENTE] WAIT\n");
                fclose(fp); close(c1);
                return 0;
            }
            if(strcmp(resp,"GO")!=0){
                // Cualquier otra cosa rompe el protocolo.
                fprintf(stderr,"PROTO: %s\n",resp);
                fclose(fp); close(c1);
                return 1;
            }
            // continuar
        } else {
            // Timeout sin línea -> server no implementa GO/WAIT
            set_timeouts(c1,5000,5000);
        }

        //  Enviar cuerpo del archivo en bloques 
        char buf[8192];
        size_t n;
        while((n=fread(buf,1,sizeof(buf),fp))>0){
            if(send(c1,buf,n,0)<0){
                perror("send body"); fclose(fp); close(c1); return 1;
            }
        }
        fclose(fp);

        // Leer respuesta final del servidor ("OK" o "WAIT")
        set_timeouts(c1,5000,5000);
        if(rdline(c1,line,sizeof(line))>0)
            printf("[CLIENTE] %s\n",line);

        close(c1);
        return 0;
    }

    // Modo desconocido
    fprintf(stderr,"Modo desconocido\n");
    close(c1);
    return 1;
}
