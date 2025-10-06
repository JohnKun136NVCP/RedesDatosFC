#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

// Verifica el puerto
static int puerto_ok(int p){ return (p==49200||p==49201||p==49202); }

// Abre un socket TCP y conecta a host:puerto 
static int conectar_tcp(const char *host, int puerto){
    int fd=-1;
    struct addrinfo h,*res=NULL,*it;
    char pstr[16];

    snprintf(pstr,sizeof(pstr),"%d",puerto);
    memset(&h,0,sizeof(h));
    h.ai_family=AF_INET;          // IPv4
    h.ai_socktype=SOCK_STREAM;    // TCP

    if(getaddrinfo(host,pstr,&h,&res)!=0) return -1;

    // Intentamos con cada dirección hasta que una conecte
    for(it=res; it; it=it->ai_next){
        fd=socket(it->ai_family,it->ai_socktype,it->ai_protocol);
        if(fd<0) continue;
        if(connect(fd,it->ai_addr,it->ai_addrlen)==0) break;   // conectado
        close(fd);                                             // probar siguiente
        fd=-1;
    }
    freeaddrinfo(res);
    return fd;  
}

// Lee el archivo completo a memoria y lo devuelve como string terminado en '\0'.
static char* leer_archivo(const char *ruta){
    FILE *f=fopen(ruta,"rb"); if(!f) return NULL;
    if(fseek(f,0,SEEK_END)!=0){ fclose(f); return NULL; }
    long n=ftell(f); if(n<0){ fclose(f); return NULL; }
    rewind(f);

    char *buf=(char*)malloc((size_t)n+1);
    if(!buf){ fclose(f); return NULL; }

    size_t r=fread(buf,1,(size_t)n,f);
    fclose(f);
    buf[r]='\0';                  // aseguramos terminación de string
    return buf;
}

// Envía exactamente n bytes por el socket 
static int enviar_todo(int fd,const char *b,size_t n){
    size_t o=0;
    while(o<n){
        ssize_t k=send(fd,b+o,n-o,0);
        if(k<=0) return -1;      // error o conexión mala
        o+=(size_t)k;
    }
    return 0;
}

// Conecta a un puerto, arma el encabezado del protocolo y envía el contenido.
static void enviar_uno(const char *host,int puerto,int objetivo,int despl,const char *contenido){
    int fd=conectar_tcp(host,puerto);
    if(fd<0){
        printf("[Cliente] No conectó %s:%d\n",host,puerto);
        return;
    }

    // Protocolo de texto que espera el servidor
    // PORT <objetivo>\n
    // SHIFT <despl>\n
    // \n
    char head[128];
    int m=snprintf(head,sizeof(head),"PORT %d\nSHIFT %d\n\n",objetivo,despl);

    // Primero el encabezado, luego el archivo
    if(enviar_todo(fd,head,(size_t)m)!=0 || enviar_todo(fd,contenido,strlen(contenido))!=0){
        printf("[Cliente] Error envío %s:%d\n",host,puerto);
        close(fd);
        return;
    }

    // Indicamos fin de datos para que el server salga y procese
    shutdown(fd,SHUT_WR);

    close(fd);
    printf("[Cliente] Puerto %d: ARCHIVO CIFRADO RECIBIDO\n", puerto);
}

int main(int argc,char **argv){
    // host + 3 puertos + 3 archivos + desplazamiento 
    if(argc < 8){
        fprintf(stderr,"Uso: %s <host> <p1> <p2> <p3> <file1> <file2> <file3> <despl>\n",argv[0]);
        return 1;
    }

    const char *host=argv[1];

    // Leemos y validamos los 3 puertos de destino
    int p[3]={atoi(argv[2]),atoi(argv[3]),atoi(argv[4])};
    if(!puerto_ok(p[0])||!puerto_ok(p[1])||!puerto_ok(p[2])){
        fprintf(stderr,"Puertos deben ser 49200 49201 49202\n");
        return 1;
    }

    // Tomamos hasta 3 archivos  y el desplazamiento es el último argumento
    const char *files[3]={NULL,NULL,NULL};
    int nfiles=0;
    for(int i=5;i<argc-1 && nfiles<3;i++) files[nfiles++]=argv[i];
    if(nfiles==0){
        fprintf(stderr,"Faltan archivos\n");
        return 1;
    }
    int despl=atoi(argv[argc-1]);

    // Mapeo 1:1 por índice:
    // files[0] -> p[0], files[1] -> p[1], files[2] -> p[2]
    for(int i=0;i<nfiles;i++){
        char *cont=leer_archivo(files[i]);
        if(!cont){
            perror("No se pudo leer archivo");
            continue;
        }
        // objetivo = mismo puerto al que enviamos (para que el server acepte)
        enviar_uno(host, p[i], p[i], despl, cont);
        free(cont);
    }

    return 0;
}
