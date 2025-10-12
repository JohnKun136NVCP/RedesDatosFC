#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define MAX_ALIAS 8 // implementé un límite razonable de aliases
#define QCAP      64 // capacidad de cada cola 
#define BUF       8192 // tamaño de buffer para copiar datos de red a disco

// implementé una cola para guardar sockets
typedef struct {
    int  fds[QCAP]; // arreglo de descriptores en espera
    int  head, tail;// índices de extracción e inserción
    int  cnt;  // cuántos elementos hay
} q_t;

// puse lo mínimo necesario para coordinarnos
static int           g_listen = -1;// socket de escucha
static int           g_port   = 49200; // puerto de servicio
static int           g_n      = 0; // número de aliases
static char          g_alias[MAX_ALIAS][64];// nombres (“s01”, “s02”…)
static char          g_ip   [MAX_ALIAS][INET_ADDRSTRLEN];// IPs resueltas
static q_t           g_q    [MAX_ALIAS]; // colas por alias

// Sincronización entre hilos: usé un mutex y varias condiciones
static pthread_mutex_t g_mtx        = PTHREAD_MUTEX_INITIALIZER;      // candado general
static pthread_cond_t  g_cv_newconn = PTHREAD_COND_INITIALIZER;       // llega nueva conexión
static pthread_cond_t  g_cv_sched   = PTHREAD_COND_INITIALIZER;       // arranque/fin de recepciones
static pthread_cond_t  g_cv_work [MAX_ALIAS];                         // despertar a cada worker

// Variables de control del semáforo de recepción
static int  g_turn = 0; // índice del alias que tiene el turno actual
static int  g_busy = 0; // bandera: 1 si un worker está recibiendo ahora
static int  g_grant[MAX_ALIAS]; // permiso que el scheduler concede a cada alias

// formateo un timestamp legible para logs
static void ts_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm);
}

//implementé un append a ~/monitor.log (o ./monitor.log si no hay HOME)
static void log_monitor(const char *msg) {
    char t[32]; ts_now(t, sizeof t);
    char logfile[512];
    const char *home = getenv("HOME");
    if (home) snprintf(logfile, sizeof logfile, "%s/monitor.log", home);
    else      snprintf(logfile, sizeof logfile, "monitor.log");
    FILE *f = fopen(logfile, "a");
    if (f) { fprintf(f, "[%s] %s\n", t, msg); fclose(f); }
}

// creo un directorio si no existía
static void ensure_dir(const char *p) {
    if (mkdir(p, 0755) == -1 && errno != EEXIST) { /* silencioso */ }
}

// sumo milisegundos a un timespec (lo uso para waits temporizados)
static void add_ms(struct timespec *ts, int ms) {
    ts->tv_sec  += ms / 1000;
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

// Inicialicé la cola circular en cero elementos
static void q_init(q_t *q){ q->head=q->tail=q->cnt=0; }

// devuelvo -1 si la cola está llena
static int  q_push(q_t *q, int fd){
    if (q->cnt == QCAP) return -1;
    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QCAP;
    q->cnt++;
    return 0;
}

// devuelvo -1 si la cola está vacía
static int  q_pop(q_t *q){
    if (q->cnt == 0) return -1;
    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QCAP;
    q->cnt--;
    return fd;
}

// Resolví un host a IPv4 en texto
static int resolve_ipv4(const char *host, char *out, size_t n) {
    struct addrinfo hints, *res=NULL;
    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res)!=0 || !res) return -1;
    inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, out, n);
    freeaddrinfo(res);
    return 0;
}

/*
  - Pregunto la dirección local del socket (getsockname)
  - Comparo con las IP de cada alias y si no hay match exacto, uso una heurística
  - Si todo falla, regreso el alias 0 como default
*/
static int local_alias_index(int sock){
    struct sockaddr_in local; socklen_t len=sizeof local;
    if (getsockname(sock, (struct sockaddr*)&local, &len) < 0) return -1;
    char lip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &local.sin_addr, lip, sizeof lip);
    for (int i=0;i<g_n;i++){
        if (strcmp(lip, g_ip[i]) == 0) return i;
    }
    for (int i=0;i<g_n;i++){
        if (strncmp(lip, g_ip[i], 10)==0) return i;
    }
    return 0;
}

/*
  - Aseguro que exista ~/ALIAS.
  - Armo un nombre: recv_YYYYmmdd_HHMMSS.dat
  - Hago un bucle de recv() -> fwrite() hasta EOF
  - Cierro y registro [RECV_END] con bytes y ruta destino
*/
static void handle_transfer(int sock, int idx){
    const char *home = getenv("HOME"); if (!home) home=".";
    for (int i=0;i<g_n;i++){ char d[512]; snprintf(d, sizeof d, "%s/%s", home, g_alias[i]); ensure_dir(d); }

    char ts[32]; time_t tt=time(NULL); struct tm tm; localtime_r(&tt,&tm);
    strftime(ts, sizeof ts, "%Y%m%d_%H%M%S", &tm);

    char path[512]; snprintf(path, sizeof path, "%s/%s/recv_%s.dat", home, g_alias[idx], ts);
    FILE *out = fopen(path, "wb"); if (!out) { perror("fopen"); close(sock); return; }

    char buf[BUF]; ssize_t r; size_t total=0;
    while ((r = recv(sock, buf, sizeof buf, 0)) > 0) {
        fwrite(buf, 1, (size_t)r, out);
        total += (size_t)r;
    }
    fclose(out);
    close(sock);

    char line[1024];
    snprintf(line, sizeof line, "[RECV_END] alias=%s bytes=%zu file=%s",
             g_alias[idx], total, path);
    puts(line); log_monitor(line);
}

/*
  - Espera a que el scheduler le dé permiso (g_grant[idx])
  - Si detecta que ya hay alguien recibiendo (g_busy) o su cola está vacía, retira el permiso y vuelve a esperar
  - Cuando tiene socket, marca g_busy=1, avisa al scheduler que arrancó y recibe todo el archivo
  - Al terminar, pone g_busy=0 y despierta al scheduler para que avance el turno
*/
static void* th_worker(void *arg){
    int idx = (int)(intptr_t)arg;
    pthread_mutex_lock(&g_mtx);
    for(;;){
        while (!g_grant[idx]) pthread_cond_wait(&g_cv_work[idx], &g_mtx);

        if (g_busy || g_q[idx].cnt==0){
            g_grant[idx]=0;
            continue;
        }
        int c = q_pop(&g_q[idx]);
        g_busy = 1;
        g_grant[idx]=0;

        pthread_cond_broadcast(&g_cv_sched); // aviso “ya estoy recibiendo”

        char msg[128]; snprintf(msg, sizeof msg, "[RECV_START] alias=%s", g_alias[idx]);
        puts(msg); log_monitor(msg);

        pthread_mutex_unlock(&g_mtx); // recibo fuera del candado
        handle_transfer(c, idx);
        pthread_mutex_lock(&g_mtx);

        g_busy = 0; // libero el “semáforo”
        pthread_cond_broadcast(&g_cv_sched); // aviso de fin
    }
    pthread_mutex_unlock(&g_mtx);
    return NULL;
}

/*
  - Si alguien está recibiendo, espero a que termine
  - Imprimo y registro quién tiene el turno
  - Si el alias en turno tiene cola, le concedo permiso y espero a que arranque y termine
  - Si no tiene trabajo, avanzo turno, si todas las colas están vacías, descansa un ratito o hasta nueva conexión
*/
static void* th_scheduler(void *arg){
    (void)arg;
    const int slice_ms = 3000; // tiempo de espera cuando no hay trabajo
    struct timespec ts;

    pthread_mutex_lock(&g_mtx);
    for(;;){
        while (g_busy) pthread_cond_wait(&g_cv_sched, &g_mtx);

        char m[64]; snprintf(m,sizeof m,"[MONITOR] turno=%s", g_alias[g_turn]);
        puts(m); log_monitor(m);

        if (g_q[g_turn].cnt > 0) {
            g_grant[g_turn] = 1; // concedo turno
            pthread_cond_signal(&g_cv_work[g_turn]);

            // espero que el worker arranque (g_busy=1) y luego termine (g_busy=0)
            clock_gettime(CLOCK_REALTIME, &ts);
            add_ms(&ts, 100);
            while (!g_busy) pthread_cond_timedwait(&g_cv_sched, &g_mtx, &ts);
            while (g_busy)  pthread_cond_wait(&g_cv_sched, &g_mtx);

            g_turn = (g_turn + 1) % g_n; // avanzo round-robin
            continue;
        }

        int ahead = (g_turn + 1) % g_n; 

        int all_empty = 1;// reviso si todos están vacíos
        for (int i=0;i<g_n;i++) if (g_q[i].cnt>0) { all_empty = 0; break; }
        if (all_empty) {
            clock_gettime(CLOCK_REALTIME, &ts);
            add_ms(&ts, slice_ms);
            pthread_cond_timedwait(&g_cv_newconn, &g_mtx, &ts);
        }

        g_turn = ahead; // roté turno aunque no hubiera trabajo
    }
    pthread_mutex_unlock(&g_mtx);
    return NULL;
}

/*
  - Acepto conexiones en g_listen
  - Detecto a qué alias llegaron (IP local) y encolo el socket en su cola
  - Despierto al scheduler para que tome decisiones
*/
static void* th_accept(void *arg){
    (void)arg;
    for(;;){
        int c = accept(g_listen, NULL, NULL);
        if (c < 0) { if (errno==EINTR) continue; perror("accept"); continue; }

        int idx = local_alias_index(c);
        pthread_mutex_lock(&g_mtx);
        if (q_push(&g_q[idx], c) == 0) {
            char t[64]; snprintf(t,sizeof t,"[ENQUEUE] alias=%s cola=%d", g_alias[idx], g_q[idx].cnt);
            puts(t); log_monitor(t);
            pthread_cond_signal(&g_cv_newconn);  // hay trabajo nuevo
            pthread_cond_broadcast(&g_cv_sched); // scheduler, despierta
        } else {
            puts("[WARN] cola llena, cerrando conexion");
            close(c);
        }
        pthread_mutex_unlock(&g_mtx);
    }
    return NULL;
}

/*
  - Parseo puerto y lista de aliases
  - Resuelvo IPs, creo ~/ALIAS, inicializo colas y condiciones
  - Creo socket de escucha (INADDR_ANY:PUERTO) y empiezo a escuchar
  - Lanzo hilos: acceptor, scheduler y un worker por alias
  - Hago join (en la práctica, el server corre indefinidamente)
*/
int main(int argc, char *argv[]){
    if (argc < 3){
        fprintf(stderr, "Uso: %s <PUERTO> <ALIAS...>\n", argv[0]);
        return 1;
    }
    g_port = atoi(argv[1]);
    g_n = 0;

    // cargo los aliases s01/s02/...
    for (int i=2; i<argc && g_n<MAX_ALIAS; i++){
        strncpy(g_alias[g_n], argv[i], sizeof g_alias[g_n]-1);
        g_alias[g_n][sizeof g_alias[g_n]-1]='\0';
        g_n++;
    }
    if (g_n == 0){ fprintf(stderr,"Debes indicar al menos un alias\n"); return 1; }

    // por cada alias, resuelvo su IP
    // creo su carpeta en HOME, inicializo su cola y su condición de trabajo
    for (int i=0;i<g_n;i++){
        if (resolve_ipv4(g_alias[i], g_ip[i], sizeof g_ip[i]) != 0) {
            snprintf(g_ip[i], sizeof g_ip[i], "%s", g_alias[i]);
        }
        const char *home = getenv("HOME"); if (!home) home=".";
        char d[512]; snprintf(d, sizeof d, "%s/%s", home, g_alias[i]);
        ensure_dir(d);
        q_init(&g_q[i]);
        pthread_cond_init(&g_cv_work[i], NULL);
    }

    // armé el socket de escucha y lo dejé listo
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen < 0){ perror("socket"); return 1; }
    int opt=1; setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in a; memset(&a,0,sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)g_port);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_listen, (struct sockaddr*)&a, sizeof a) < 0){
        perror("bind"); close(g_listen); return 1;
    }
    if (listen(g_listen, 64) < 0){
        perror("listen"); close(g_listen); return 1;
    }

    // presento en pantalla qué alias/IPs estoy atendiendo
    printf("[SERVER] Escuchando en %d para ", g_port);
    for (int i=0;i<g_n;i++) printf("%s(%s)%s", g_alias[i], g_ip[i], (i==g_n-1)?"\n":", ");

    // lancé los hilos: acceptor, scheduler y un worker por alias
    pthread_t th_acc, th_sch, th_w[MAX_ALIAS];
    pthread_create(&th_acc, NULL, th_accept, NULL);
    pthread_create(&th_sch, NULL, th_scheduler, NULL);
    for (int i=0;i<g_n;i++){
        pthread_create(&th_w[i], NULL, th_worker, (void*)(intptr_t)i);
    }

    // espero
    pthread_join(th_acc, NULL);
    pthread_join(th_sch, NULL);
    for (int i=0;i<g_n;i++) pthread_join(th_w[i], NULL);

    close(g_listen);
    return 0;
}
