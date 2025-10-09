#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SERVERS 4   // numero maximo de servidores
#define BUF 8192        // tamaño de buffer para recepcion
#define QCAP 64         // capacidad de la cola

// estructura de cola para sockets
typedef struct {
    int fds[QCAP];
    int head, tail;
    int cnt;
} queue_t;


static int g_listen = -1;               // socket de escucha
static int g_turn = 0;                  // indice del servidor que tiene el turno
static int g_busy = 0;                  // indicador si algun worker esta recibiendo
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;  // mutex general
static pthread_cond_t g_cv_turn = PTHREAD_COND_INITIALIZER; // condicion para controlar turno
static queue_t g_q[MAX_SERVERS];        // colas por servidor

// inicializa la cola
static void q_init(queue_t *q){ 
    q->head = q->tail = q->cnt = 0; 
}

// inserta un socket en la cola, devuelve -1 si esta llena
static int q_push(queue_t *q, int fd){
    if (q->cnt == QCAP) return -1;
    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QCAP;
    q->cnt++;
    return 0;
}

// extrae un socket de la cola, devuelve -1 si esta vacia
static int q_pop(queue_t *q){
    if (q->cnt == 0) return -1;
    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QCAP;
    q->cnt--;
    return fd;
}

// recibe datos de un socket y los guarda en un archivo
static void handle_transfer(int sock, int idx){
    char buf[BUF]; ssize_t r;

    // genera nombre de archivo con fecha y hora
    char filename[128];
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t,&tm);
    snprintf(filename,sizeof filename,"recv_server%d_%04d%02d%02d_%02d%02d%02d.txt",
             idx, tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    FILE *f = fopen(filename,"wb"); 
    if(!f){ perror("fopen"); close(sock); return; }

    size_t total = 0;
    while((r = recv(sock, buf, sizeof buf, 0))>0){ // bucle de recepcion
        fwrite(buf,1,(size_t)r,f); 
        total += (size_t)r;
    }

    fclose(f);
    close(sock); 
    printf("[SERVER %d] Recibidos %zu bytes en %s\n", idx, total, filename);
}

// hilo worker para cada servidor
static void* worker(void *arg){
    int idx = (int)(intptr_t)arg; // indice del servidor
    for(;;){
        pthread_mutex_lock(&g_mtx);
        // espera hasta que sea su turno y haya sockets en la cola
        while(idx != g_turn || g_q[idx].cnt == 0) 
            pthread_cond_wait(&g_cv_turn, &g_mtx);

        g_busy = 1; 
        int c = q_pop(&g_q[idx]); 
        pthread_mutex_unlock(&g_mtx);

        handle_transfer(c, idx); 

        pthread_mutex_lock(&g_mtx);
        g_busy = 0;              
        g_turn = (g_turn + 1) % MAX_SERVERS; // pasa turno al siguiente
        pthread_cond_broadcast(&g_cv_turn);  // despierta a los demas workers
        pthread_mutex_unlock(&g_mtx);
    }
    return NULL;
}

// hilo que acepta conexiones entrantes
static void* acceptor(void *arg){
    (void)arg;
    for(;;){
        int c = accept(g_listen, NULL, NULL); // acepta conexion
        if(c<0){ 
            if(errno==EINTR) continue; 
            perror("accept"); 
            continue; 
        }

        pthread_mutex_lock(&g_mtx);
        // inserta socket en la cola del servidor actual
        if(q_push(&g_q[g_turn], c)==0)
            pthread_cond_broadcast(&g_cv_turn); 
        else
            close(c);
        pthread_mutex_unlock(&g_mtx);
    }
    return NULL;
}

// funcion principal
int main(int argc, char *argv[]){
    if(argc < 2){ fprintf(stderr,"Uso: %s <PUERTO>\n",argv[0]); return 1; }

    int port = atoi(argv[1]);

    // inicializa colas
    for(int i=0;i<MAX_SERVERS;i++) q_init(&g_q[i]);

    // crea socket de escucha
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    if(g_listen<0){ perror("socket"); return 1; }

    int opt=1;
    setsockopt(g_listen,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);

    struct sockaddr_in addr;
    memset(&addr,0,sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(g_listen,(struct sockaddr*)&addr,sizeof addr)<0){ perror("bind"); return 1; }
    if(listen(g_listen,64)<0){ perror("listen"); return 1; }

    printf("Servidor escuchando en puerto %d\n",port);

    pthread_t th_acc;
    pthread_t th_w[MAX_SERVERS];

    pthread_create(&th_acc,NULL,acceptor,NULL); // hilo aceptor

    for(int i=0;i<MAX_SERVERS;i++) // hilos workers
        pthread_create(&th_w[i],NULL,worker,(void*)(intptr_t)i);

    pthread_join(th_acc,NULL); // espera hilo aceptor
    for(int i=0;i<MAX_SERVERS;i++) pthread_join(th_w[i],NULL); // espera workers

    close(g_listen);
    return 0;
}
