#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE    8192
#define NUM_SERVERS    4
#define TURNO_TIMEOUT  10      // segundos (ventana para iniciar; si ya empezó, termina)

// ======= Mapeo de alias a IPs =======
typedef struct {
    const char *alias;
    const char *ip;   // si es NULL, bind a INADDR_ANY
    int port;
    int index;        // 0..3
} alias_info_t;

static alias_info_t ALIASES[NUM_SERVERS] = {
    { "s01", "192.168.0.101", 49200, 0 },
    { "s02", "192.168.0.102", 49200, 1 },
    { "s03", "192.168.0.103", 49200, 2 },
    { "s04", "192.168.0.104", 49200, 3 },
};

/* =================== Monitor global para sincronización Round Robin =================== */
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;

static int  current_turn = 0;                  // a quién le toca (0..3)
static int  receiving    = 0;                  // 1 si alguien está recibiendo
static int  pending_req[NUM_SERVERS] = {0};    // 1 si ese hilo ya aceptó y espera turno
static int  running      = 1;

// ======= Función de Cifrado César de práctica 2 =======
static void encrypt_cesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i]; i++) {
        unsigned char c = (unsigned char) text[i];
        if (isupper(c)) text[i] = (char)('A' + (c - 'A' + shift) % 26);
        else if (islower(c)) text[i] = (char)('a' + (c - 'a' + shift) % 26);
    }
}

// ======= Helper para escribir logs con timestamp =======
static void log_event(const char *alias, const char *msg) {
    struct stat st = {0};
    if (stat(alias, &st) == -1) mkdir(alias, 0755);
    char path[256];
    snprintf(path, sizeof(path), "%s/status.log", alias);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[32]; strftime(ts, sizeof(ts), "%F %T", localtime(&now));
    fprintf(f, "[%s] %s\n", ts, msg);
    fclose(f);
}

/* =================== Hilo timer: gestión automática de turnos =================== */
//Rotamos solo en  caso que nadie este recibiendo y el turno actual no tenga pendiente.
static void *turn_timer_thread(void *arg) {
    (void)arg;
    while (running) {
        sleep(TURNO_TIMEOUT);
        pthread_mutex_lock(&m);
        if (!receiving && pending_req[current_turn] == 0) {
            int next = (current_turn + 1) % NUM_SERVERS;
            printf("[*]CONTROL → Turno pasado a %s (índice: %d)\n",
                   ALIASES[next].alias, next);
            current_turn = next;
            pthread_cond_broadcast(&cv);
        }
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

/* =================== Hilo worker por servidor =================== */
typedef struct {
    alias_info_t info;
} worker_args_t;

static void *alias_worker(void *arg) {
    worker_args_t *wa = (worker_args_t*)arg;
    const char *alias = wa->info.alias;
    const char *ip    = wa->info.ip;
    int port          = wa->info.port;
    int idx           = wa->info.index;

    // ======= Crear socket TCP =======
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return NULL; }

    // ======= Reusar puerto si se reinicia rápido =======
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ======= Configuración del servidor =======
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    if (ip && inet_pton(AF_INET, ip, &sa.sin_addr) == 1) {
        // bind a IP específica
    } else {
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    sa.sin_port = htons(port);

    // ======= Bind =======
    if (bind(listenfd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("bind");
        close(listenfd);
        return NULL;
    }

    // ======= Listen =======
    if (listen(listenfd, 8) < 0) {
        perror("listen");
        close(listenfd);
        return NULL;
    }

    printf("[*]SERVIDOR %s escuchando en puerto %d (índice: %d)\n", alias, port, idx);
    log_event(alias, "Servidor iniciado y en escucha");

    // ======= Loop principal para aceptar conexiones =======
    while (running) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            if (!running) break;
            perror("accept");
            continue;
        }

        printf("[*]SERVIDOR %s → Solicitud aceptada\n", alias);

        // Señalamos que tenemos trabajo pendiente
        pthread_mutex_lock(&m);
        pending_req[idx] = 1;

        // Esperamos hasta que sea mi turno y nadie esté recibiendo
        while (running && (current_turn != idx || receiving)) {
            pthread_cond_wait(&cv, &m);
        }

        if (!running) { pthread_mutex_unlock(&m); close(connfd); break; }

        // Tomamos la "posesión" de la recepción solo uno a la vez
        receiving = 1;
        pending_req[idx] = 0;
        pthread_mutex_unlock(&m);

        // ======= Leer petición: "<PUERTO_OBJ> <SHIFT> <TEXTO>" =======
        char buffer[BUFFER_SIZE]; memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(connfd, buffer, sizeof(buffer)-1);
        if (n <= 0) {
            close(connfd);
            pthread_mutex_lock(&m);
            receiving = 0;
            current_turn = (current_turn + 1) % NUM_SERVERS;
            pthread_cond_broadcast(&cv);
            pthread_mutex_unlock(&m);
            continue;
        }

        int puerto_obj = 0, shift = 0;
        char texto[BUFFER_SIZE]; texto[0] = '\0';

        if (sscanf(buffer, "%d %d %[^\n]", &puerto_obj, &shift, texto) < 2) {
            const char *errorFormato = "Formato inválido\n";
            write(connfd, errorFormato, strlen(errorFormato));
            log_event(alias, "Solicitud rechazada: formato inválido");
            close(connfd);
        } else if (puerto_obj != port) {
            // Rechazo
            char rechazo[128];
            snprintf(rechazo, sizeof(rechazo), "Rechazado por servidor %s\n", alias);
            write(connfd, rechazo, strlen(rechazo));
            log_event(alias, "Solicitud rechazada: puerto incorrecto");
            close(connfd);
        } else {
            // Aceptado
            log_event(alias, "Archivo aceptado y en proceso");
            encrypt_cesar(texto, shift);

            printf("[*]SERVIDOR %s → Archivo recibido y cifrado: %s\n", alias, texto);

            // ======= Responder al cliente =======
            char respuesta[BUFFER_SIZE];
            snprintf(respuesta, sizeof(respuesta),
                     "Procesado por servidor %s en puerto %d: %.4000s\n",
                     alias, port, texto);
            write(connfd, respuesta, strlen(respuesta));
            close(connfd);
            log_event(alias, "Archivo cifrado correctamente");
        }

        // Liberamos la recepción y pasamos el turno (round-robin)
        pthread_mutex_lock(&m);
        receiving = 0;
        current_turn = (current_turn + 1) % NUM_SERVERS;
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&m);
    }

    close(listenfd);
    return NULL;
}

/* =================== Handler para señales =================== */
static void on_signal(int sig) {
    (void)sig;
    running = 0;
    pthread_mutex_lock(&m);
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&m);
}

// ======= Servidor con sistema Round Robin =======
int main(void) {
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    current_turn = 0; // turno inicial s01

    // Timer para rotar el turno cuando nadie recibe y no hay pendiente
    pthread_t timer_th;
    pthread_create(&timer_th, NULL, turn_timer_thread, NULL);

    // Lanzar 4 hilos de alias
    pthread_t workers[NUM_SERVERS];
    worker_args_t args[NUM_SERVERS];
    for (int i = 0; i < NUM_SERVERS; i++) {
        args[i].info = ALIASES[i];
        pthread_create(&workers[i], NULL, alias_worker, &args[i]);
    }

    // Esperar workers
    for (int i = 0; i < NUM_SERVERS; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_join(timer_th, NULL);
    return 0;
}
