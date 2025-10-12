#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//Definimos algunos valores maximos
#define MAX_ALIASES 8
#define COLA_MAX 64
#define CAB_MAX 1024 //tamaño del buffer
#define BLOQUE 65536 //tamaño para recibir archivos
#define BACKLOG 64
#define RANGO_EFIM 200 // rango de puertos
#define ESPERA_IDLE 200 // tiempo de espera

// Sirve para guardar conexiones que esperan ser atendidas
typedef struct {
    int v[COLA_MAX];
    int h, t, n;
} cola_t;

static void cola_init(cola_t *c){ c->h=c->t=c->n=0; }
static int  cola_push(cola_t *c, int fd){
    if (c->n==COLA_MAX) return -1;
    c->v[c->t] = fd; c->t = (c->t+1)%COLA_MAX; c->n++; return 0;
}
static int  cola_pop(cola_t *c){
    if (c->n==0) return -1;
    int fd = c->v[c->h]; c->h = (c->h+1)%COLA_MAX; c->n--; return fd;
}
static int  puerto_base = 49200;
static int  fd_base = -1;
static int  N = 0; // numero de alias
static char aliasN[MAX_ALIASES][64];
static char aliasIP[MAX_ALIASES][INET_ADDRSTRLEN];
static cola_t colas[MAX_ALIASES];

// Variables para sincronizar los hilos
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv_nueva = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  cv_core  = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  cv_permiso[MAX_ALIASES];

// Variables de control para los turnos
static int turno   = 0;
static int ocupado = 0; //quien esta recibiendo
static int ticket[MAX_ALIASES];// quien tiene el permiso

// Crea un directorio por si acaso
static void mk_dir(const char *p){
    if (mkdir(p,0755)==-1 && errno!=EEXIST) {}
}
static int resolver_ip(const char *host, char *out, size_t n){
    struct addrinfo hi,*res=NULL; memset(&hi,0,sizeof hi);
    hi.ai_family=AF_INET; hi.ai_socktype=SOCK_STREAM;
    if (getaddrinfo(host,NULL,&hi,&res)!=0 || !res) return -1;
    inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, out, n);
    freeaddrinfo(res); return 0;
}
static int leer_linea(int fd, char *buf, int max){
    int i=0; while(i<max-1){ char c; int r=recv(fd,&c,1,0); if(r==0) break; if(r<0) return -1; buf[i++]=c; if(c=='\n') break; }
    buf[i]=0; return i;
}

// Busca qué alias corresponde a un socket
static int alias_para_socket(int sock){
    struct sockaddr_in loc; socklen_t L=sizeof loc;
    if (getsockname(sock,(struct sockaddr*)&loc,&L)<0) return 0;
    char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&loc.sin_addr,ip,sizeof ip);
    for(int i=0;i<N;i++) if(strcmp(ip,aliasIP[i])==0) return i;
    for(int i=0;i<N;i++) if(strncmp(ip,aliasIP[i],10)==0) return i;
    return 0;
}

// Abre un puerto
static int abrir_efimero(int *pout){
    for (int p=puerto_base+1; p<=puerto_base+RANGO_EFIM; ++p){
        int fd = socket(AF_INET,SOCK_STREAM,0); if(fd<0) return -1;
        int on=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof on);
        struct sockaddr_in a; memset(&a,0,sizeof a);
        a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY; a.sin_port=htons((unsigned short)p);
        if (bind(fd,(struct sockaddr*)&a,sizeof a)==0 && listen(fd,BACKLOG)==0){ *pout=p; return fd; }
        close(fd);
    }
    return -1;
}

// Recibe archivo
static void recibir_archivo(int cfd, int idx_alias){
    char linea[CAB_MAX]; int tam=0; char nombre[CAB_MAX]={0};

    if (leer_linea(cfd,linea,sizeof linea)<=0){ close(cfd); return; }
    sscanf(linea,"LEN:%d",&tam);

    if (leer_linea(cfd,linea,sizeof linea)<=0){ close(cfd); return; }
    if (!strncmp(linea,"NAME:",5)){
        strncpy(nombre,linea+5,sizeof(nombre)-1);
        size_t L=strlen(nombre); if(L && nombre[L-1]=='\n') nombre[L-1]=0;
    }
    leer_linea(cfd,linea,sizeof linea);

    if (tam<=0 || tam>(1<<26)){ send(cfd,"ERR\n",4,0); close(cfd); return; }

    const char *home=getenv("HOME"); if(!home) home=".";
    char dir[512]; snprintf(dir,sizeof dir,"%s/%s",home,aliasN[idx_alias]); mk_dir(dir);

    //Si no hay un nombre se genera uno
    char def[64]; if(!nombre[0]){
        time_t t=time(NULL); struct tm tm; localtime_r(&t,&tm);
        strftime(def,sizeof def,"recv_%Y%m%d_%H%M%S.dat",&tm);
        strcpy(nombre,def);
    }

    // Crear ruta final
    char salida[768]; snprintf(salida,sizeof salida,"%s/%s",dir,nombre);

    // Log de inicio
    printf("[RR][%s] RECIBIENDO %s (%d B)\n", aliasN[idx_alias], nombre, tam);
    fflush(stdout);

    // Escribir archivo recibido
    FILE *fp=fopen(salida,"wb"); if(!fp){ send(cfd,"ERR\n",4,0); close(cfd); return; }
    char *buf=(char*)malloc(BLOQUE); if(!buf){ fclose(fp); close(cfd); return; }
    int faltan=tam;
    while(faltan>0){
        int p = faltan>BLOQUE ? BLOQUE : faltan;
        int r = recv(cfd,buf,p,0);
        if (r<=0){ free(buf); fclose(fp); close(cfd); return; }
        fwrite(buf,1,(size_t)r,fp);
        faltan -= r;
    }
    free(buf); fclose(fp);
    send(cfd,"OK\n",3,0); close(cfd);

    // Completado
    printf("[RR][%s] COMPLETADO %s (%d B)\n", aliasN[idx_alias], salida, tam);
    fflush(stdout);
}

// Worker por alias
static void* th_worker(void *arg){
    int yo = (int)(intptr_t)arg;

    pthread_mutex_lock(&mtx);
    for(;;){
        while(!ticket[yo]) pthread_cond_wait(&cv_permiso[yo], &mtx);

        if (colas[yo].n==0 || ocupado){
            ticket[yo]=0;
            continue;
        }
        int cli = cola_pop(&colas[yo]);
        ticket[yo]=0;
        ocupado=1; pthread_cond_broadcast(&cv_core);

        pthread_mutex_unlock(&mtx);
        recibir_archivo(cli, yo);
        pthread_mutex_lock(&mtx);

        ocupado=0; pthread_cond_broadcast(&cv_core);
    }
    pthread_mutex_unlock(&mtx);
    return NULL;
}

// Planificador RR
static void* th_rr(void *arg){
    (void)arg;
    struct timespec ts;

    pthread_mutex_lock(&mtx);
    for(;;){
        while(ocupado) pthread_cond_wait(&cv_core, &mtx);

        if (colas[turno].n > 0){
            printf("[RR] TURNO de %s \n", aliasN[turno]);
            fflush(stdout);
            ticket[turno]=1;
            pthread_cond_signal(&cv_permiso[turno]);
            clock_gettime(CLOCK_REALTIME,&ts);
            ts.tv_nsec += 100L*1000000L; if(ts.tv_nsec>=1000000000L){ ts.tv_sec++; ts.tv_nsec-=1000000000L; }
            while(!ocupado) pthread_cond_timedwait(&cv_core,&mtx,&ts);
            while( ocupado) pthread_cond_wait(&cv_core,&mtx);
            turno = (turno+1)%N;
            continue;
        }

        turno = (turno+1)%N;
    }
    pthread_mutex_unlock(&mtx);
    return NULL;
}

// Aceptador
static void* th_accept(void *arg){
    (void)arg;
    for(;;){
        int c0 = accept(fd_base,NULL,NULL);
        if (c0<0){ if(errno==EINTR) continue; perror("accept base"); continue; }
        int ptrab=0;
        int fdw = abrir_efimero(&ptrab);
        if (fdw<0){
            send(c0,"PORT:0\n\n",8,0);
            close(c0);
            continue;
        }
        char ans[64]; int n=snprintf(ans,sizeof ans,"PORT:%d\n\n",ptrab);
        send(c0,ans,n,0); close(c0);

        int cs = accept(fdw,NULL,NULL);
        close(fdw);
        if (cs<0){ perror("accept efimero"); continue; }
        int idx = alias_para_socket(cs);
        pthread_mutex_lock(&mtx);
        if (cola_push(&colas[idx], cs)==0){
            pthread_cond_signal(&cv_nueva);
            pthread_cond_broadcast(&cv_core);
        } else {
            close(cs);
        }
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

// Main
int main(int argc, char **argv){
    if (argc<3){
        fprintf(stderr,"Uso: %s <PUERTO_BASE> <ALIAS...>\n", argv[0]);
        return 1;
    }
    puerto_base = atoi(argv[1]);
    if (puerto_base<=0){ fprintf(stderr,"Puerto inválido\n"); return 1; }

    N=0;
    for(int i=2;i<argc && N<MAX_ALIASES;i++){
        strncpy(aliasN[N], argv[i], sizeof aliasN[N]-1);
        aliasN[N][sizeof aliasN[N]-1]=0;
        N++;
    }
    if (N==0){ fprintf(stderr,"Debes pasar al menos un alias\n"); return 1; }
    for(int i=0;i<N;i++){
        if (resolver_ip(aliasN[i], aliasIP[i], sizeof aliasIP[i])!=0){
            snprintf(aliasIP[i], sizeof aliasIP[i], "%s", aliasN[i]);
        }
        const char *home=getenv("HOME"); if(!home) home=".";
        char dir[512]; snprintf(dir,sizeof dir,"%s/%s",home,aliasN[i]); mk_dir(dir);
        cola_init(&colas[i]);
        pthread_cond_init(&cv_permiso[i], NULL);
        ticket[i]=0;
    }
    fd_base = socket(AF_INET,SOCK_STREAM,0);
    if (fd_base<0){ perror("socket"); return 1; }
    int on=1; setsockopt(fd_base,SOL_SOCKET,SO_REUSEADDR,&on,sizeof on);

    struct sockaddr_in a; memset(&a,0,sizeof a);
    a.sin_family=AF_INET; a.sin_port=htons((unsigned short)puerto_base);
    a.sin_addr.s_addr=INADDR_ANY;
    if (bind(fd_base,(struct sockaddr*)&a,sizeof a)<0){ perror("bind"); close(fd_base); return 1; }
    if (listen(fd_base,BACKLOG)<0){ perror("listen"); close(fd_base); return 1; }
    for(int i=0;i<N;i++){
        printf("[+] Escuchando en %s:%d (%s)\n", aliasIP[i], puerto_base, aliasN[i]);
    }
    fflush(stdout);

    // lanzar hilos
    pthread_t thA, thR, thW[MAX_ALIASES];
    pthread_create(&thA, NULL, th_accept, NULL);
    pthread_create(&thR, NULL, th_rr, NULL);
    for(int i=0;i<N;i++) pthread_create(&thW[i], NULL, th_worker, (void*)(intptr_t)i);
    pthread_join(thA, NULL);
    pthread_join(thR, NULL);
    for(int i=0;i<N;i++) pthread_join(thW[i], NULL);

    close(fd_base);
    return 0;
}

