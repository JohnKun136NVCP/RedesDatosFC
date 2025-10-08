
#define _DEFAULT_SOURCE 1
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SLICE_MS 2000
#define BACKLOG  16
#define BUFSZ    65536

typedef struct {
  int base_port;
  const char *alias;
  int my_id;
  int total;
} worker_args_t;

// ===== RR globals =====
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv = PTHREAD_COND_INITIALIZER;
static int g_turn = 0;
static int g_running = 1;

// ===== helpers =====
static void sleep_ms(long ms) {
  struct timespec ts;
  ts.tv_sec  = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) && errno == EINTR) { /* retry */ }
}

static int set_nonblock(int fd) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0) return -1;
  return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static uint64_t ntohll_u64(uint64_t x) {
  uint32_t hi = ntohl((uint32_t)(x >> 32));
  uint32_t lo = ntohl((uint32_t)(x & 0xffffffffULL));
  return ((uint64_t)lo << 32) | hi;
}

static ssize_t sendn(int fd, const void *buf, size_t n) {
  size_t sent = 0; const char *p = (const char*)buf;
  while (sent < n) {
    ssize_t w = send(fd, p + sent, n - sent, 0);
    if (w < 0) { if (errno == EINTR) continue; return -1; }
    sent += (size_t)w;
  }
  return (ssize_t)sent;
}

static ssize_t recvn(int fd, void *buf, size_t n) {
  size_t got = 0; char *p = (char*)buf;
  while (got < n) {
    ssize_t r = recv(fd, p + got, n - got, 0);
    if (r == 0) return got;
    if (r < 0) { if (errno == EINTR) continue; return -1; }
    got += (size_t)r;
  }
  return (ssize_t)got;
}

// ===== protocolo =====
typedef struct __attribute__((packed)) {
  uint32_t name_len_be;
  uint64_t total_be;
} hdr_t;

// ===== sockets auxiliares =====
static int make_listener_for_alias(const char *alias, int port) {
  fprintf(stderr, "[%s] worker start: resolviendo alias...\n", alias);

  struct hostent *he = gethostbyname(alias);
  if (!he) {
    // En algunas distros hstrerror/h_errno no están expuestos; damos mensaje genérico
    fprintf(stderr, "[%s] gethostbyname() falló\n", alias);
    return -1;
  }

  struct in_addr resolved = *(struct in_addr*)he->h_addr_list[0];
  fprintf(stderr, "[%s] resuelto a %s; intentando bind en puerto %d\n",
          alias, inet_ntoa(resolved), port);

  int ls = socket(AF_INET, SOCK_STREAM, 0);
  if (ls < 0) { fprintf(stderr, "[%s] socket() falló: %s\n", alias, strerror(errno)); return -1; }
  int opt = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(port);
  memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);

  if (bind(ls, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    fprintf(stderr, "[%s] bind(%s:%d) falló: %s (errno=%d)\n",
            alias, inet_ntoa(resolved), port, strerror(errno), errno);
    close(ls);
    return -1;
  }
  if (listen(ls, BACKLOG) < 0) {
    fprintf(stderr, "[%s] listen() falló: %s\n", alias, strerror(errno));
    close(ls);
    return -1;
  }
  return ls;
}

static int make_data_listener_same_ip(const char *alias, int *out_port) {
  struct hostent *he = gethostbyname(alias);
  if (!he) return -1;

  int ls = socket(AF_INET, SOCK_STREAM, 0);
  if (ls < 0) return -1;
  int opt = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in sa; socklen_t sl = sizeof(sa);
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(0); // efímero
  memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);

  if (bind(ls, (struct sockaddr*)&sa, sizeof(sa)) < 0) { close(ls); return -1; }
  if (listen(ls, 1) < 0) { close(ls); return -1; }
  if (getsockname(ls, (struct sockaddr*)&sa, &sl) == 0)
    *out_port = ntohs(sa.sin_port);
  else { close(ls); return -1; }

  return ls;
}

// ===== worker =====
static void *worker(void *arg) {
  worker_args_t *w = (worker_args_t*)arg;
  const char *alias = w->alias;
  int base = w->base_port;
  int id = w->my_id;
  int total = w->total;

  int ls = make_listener_for_alias(alias, base);
  if (ls < 0) {
    fprintf(stderr, "[%s] no pude crear listener base; aborto hilo\n", alias);
    return NULL;
  }
  set_nonblock(ls);
  printf("[%s] CONTROL listo en %s:%d\n", alias, alias, base);

  while (g_running) {
    // Round-robin: esperar turno
    pthread_mutex_lock(&g_mx);
    while (g_turn != id && g_running)
      pthread_cond_wait(&g_cv, &g_mx);
    pthread_mutex_unlock(&g_mx);
    if (!g_running) break;

    // Ventana de turno
    fd_set rf; FD_ZERO(&rf); FD_SET(ls, &rf);
    struct timeval tv = { .tv_sec = SLICE_MS/1000, .tv_usec = (SLICE_MS%1000)*1000 };
    int sel = select(ls+1, &rf, NULL, NULL, &tv);

    if (sel <= 0) { // no hubo cliente: ceder turno
      pthread_mutex_lock(&g_mx);
      g_turn = (g_turn + 1) % total;
      pthread_cond_broadcast(&g_cv);
      pthread_mutex_unlock(&g_mx);
      continue;
    }

    // Aceptar control
    struct sockaddr_in cli; socklen_t cl = sizeof(cli);
    int c = accept(ls, (struct sockaddr*)&cli, &cl);
    if (c < 0) continue;

    // Preparar listener de datos efímero en la misma IP
    int data_port = 0;
    int lsd = make_data_listener_same_ip(alias, &data_port);
    if (lsd < 0) { close(c); continue; }

    char portbuf[32]; snprintf(portbuf, sizeof(portbuf), "%d", data_port);
    send(c, portbuf, strlen(portbuf), 0);
    close(c);

    // Aceptar datos (bloqueante: recibe TODO)
    int d = accept(lsd, (struct sockaddr*)&cli, &cl);
    close(lsd);
    if (d < 0) continue;

    // Cabecera
    hdr_t h;
    if (recvn(d, &h, sizeof(h)) != (ssize_t)sizeof(h)) { close(d); goto after_recv; }
    uint32_t name_len   = ntohl(h.name_len_be);
    uint64_t total_bytes= ntohll_u64(h.total_be);
    if (name_len == 0 || name_len > 4096) { close(d); goto after_recv; }

    // Nombre de archivo
    char *name = (char*)malloc(name_len + 1);
    if (!name) { close(d); goto after_recv; }
    if (recvn(d, name, name_len) != (ssize_t)name_len) { free(name); close(d); goto after_recv; }
    name[name_len] = '\0';

    // Ruta destino: $HOME/<alias>/<name>
    const char *home = getenv("HOME"); if (!home) home = ".";
    char dirpath[512]; snprintf(dirpath, sizeof(dirpath), "%s/%s", home, alias);
    char cmd[600]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dirpath); system(cmd);
    char filepath[1024]; snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, name);

    FILE *f = fopen(filepath, "wb");
    if (!f) { free(name); close(d); goto after_recv; }

    // Cuerpo del archivo
    char *buf = (char*)malloc(BUFSZ);
    uint64_t recvd = 0;
    while (recvd < total_bytes) {
      size_t want = (total_bytes - recvd) < BUFSZ ? (size_t)(total_bytes - recvd) : BUFSZ;
      ssize_t r = recvn(d, buf, want);
      if (r <= 0) break;
      fwrite(buf, 1, (size_t)r, f);
      recvd += (uint64_t)r;
    }
    free(buf);
    fclose(f);

    // Confirmación
    char ok[1024];
    snprintf(ok, sizeof(ok), "OK %llu %s\n",
             (unsigned long long)recvd, filepath);
    sendn(d, ok, strlen(ok));
    close(d);

    printf("[%s] Archivo recibido: %s (%llu bytes)\n",
           alias, filepath, (unsigned long long)recvd);
    free(name);

  after_recv:
    // Ceder turno
    pthread_mutex_lock(&g_mx);
    g_turn = (g_turn + 1) % w->total;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mx);
  }

  close(ls);
  return NULL;
}

// ===== señal =====
static void on_sigint(int s) {
  (void)s;
  pthread_mutex_lock(&g_mx);
  g_running = 0;
  pthread_cond_broadcast(&g_cv);
  pthread_mutex_unlock(&g_mx);
}

// ===== main =====
int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <PUERTO_BASE> <alias1> [alias2 ...]\n", argv[0]);
    return 1;
  }

  // stdout sin buffer: imprime todo al instante
  setvbuf(stdout, NULL, _IONBF, 0);
  signal(SIGINT, on_sigint);

  int base  = atoi(argv[1]);
  int total = argc - 2;
  if (total <= 0 || total > 16) {
    fprintf(stderr, "Debes proporcionar entre 1 y 16 alias\n");
    return 1;
  }

  pthread_t th[16];
  worker_args_t args[16];

  fprintf(stderr, "[main] creando %d hilos...\n", total);
  for (int i = 0; i < total; ++i) {
    args[i].base_port = base;
    args[i].alias     = argv[2 + i];
    args[i].my_id     = i;
    args[i].total     = total;

    fprintf(stderr, "[main] pthread_create #%d alias=%s ...\n", i, args[i].alias);
    int rc = pthread_create(&th[i], NULL, worker, &args[i]);
    if (rc != 0) {
      errno = rc;
      fprintf(stderr, "[main] pthread_create fallo para %s: %s (rc=%d)\n",
              args[i].alias, strerror(errno), rc);
      return 1;
    }
    // Stagger para evitar carreras en bind
    sleep_ms(500);
    fprintf(stderr, "[main] hilo #%d lanzado (%s)\n", i, args[i].alias);
  }
  fprintf(stderr, "[main] todos los pthread_create() retornaron OK\n");

  for (int i = 0; i < total; ++i) pthread_join(th[i], NULL);
  return 0;
}
