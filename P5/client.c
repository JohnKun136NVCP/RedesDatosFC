#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUF 65536

// ---- util 64-bit net/host ----
static uint64_t htonll_u64(uint64_t x) {
  uint32_t hi = htonl((uint32_t)(x >> 32));
  uint32_t lo = htonl((uint32_t)(x & 0xffffffffULL));
  return ((uint64_t)lo << 32) | hi;
}
static uint64_t ntohll_u64(uint64_t x) {
  uint32_t hi = ntohl((uint32_t)(x >> 32));
  uint32_t lo = ntohl((uint32_t)(x & 0xffffffffULL));
  return ((uint64_t)lo << 32) | hi;
}

static int connect_to(const char *host, int port) {
  struct hostent *he = gethostbyname(host);
  if (!he) return -1;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;
  struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET; sa.sin_port = htons(port);
  // usar h_addr_list[0] (no h_addr)
  memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
  if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) < 0) { close(s); return -1; }
  return s;
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

static void log_line(const char *line) {
  FILE *f = fopen("cliente_log.txt", "a");
  if (!f) return;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
  fprintf(f, "[%s] %s\n", ts, line);
  fclose(f);
  printf("[%s] %s\n", ts, line);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "Uso: %s <alias> <puerto_base> <status|upload> [archivo]\n", argv[0]);
    return 1;
  }
  const char *host = argv[1];
  int base = atoi(argv[2]);
  const char *op = argv[3];

  // 1) Handshake CONTROL: pedir puerto de DATOS
  int c = connect_to(host, base);
  if (c < 0) { perror("connect control"); return 1; }

  char portbuf[32];
  int n = recv(c, portbuf, sizeof(portbuf)-1, 0);
  if (n <= 0) { fprintf(stderr, "No recibí puerto de datos\n"); close(c); return 1; }
  portbuf[n] = '\0';
  int dataport = atoi(portbuf);
  close(c);

  // 2) Conectar a DATOS
  int d = connect_to(host, dataport);
  if (d < 0) { perror("connect data"); return 1; }

  // Cabecera de datos
  struct __attribute__((packed)) { uint32_t name_len_be; uint64_t total_be; } hdr;

  if (!strcmp(op, "status")) {
    const char *name = "STATUS.txt";
    uint64_t total = 0ULL;

    hdr.name_len_be = htonl((uint32_t)strlen(name));
    hdr.total_be    = htonll_u64(total);

    if (sendn(d, &hdr, sizeof(hdr)) < 0) { perror("send hdr"); close(d); return 1; }
    if (sendn(d, name, strlen(name)) < 0) { perror("send name"); close(d); return 1; }

    // no hay payload (0 bytes)
    shutdown(d, SHUT_WR);

    // esperar confirmación
    char ans[256]; int r = recv(d, ans, sizeof(ans)-1, 0);
    if (r > 0) {
      ans[r] = 0;
      // Mostrar formato amable
      // Si el servidor responde "OK ...", imprimimos "Servidor en espera"
      if (strncmp(ans, "OK", 2) == 0) log_line("Respuesta del servidor: Servidor en espera");
      else log_line(ans);
    }
    close(d);
    return 0;
  }

  if (!strcmp(op, "upload")) {
    if (argc < 5) { fprintf(stderr,"Falta archivo\n"); close(d); return 1; }
    const char *path = argv[4];

    // nombre base
    const char *name = strrchr(path, '/'); name = (name ? name+1 : path);

    // tamaño
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
      fprintf(stderr, "Archivo inválido: %s\n", path);
      close(d); return 1;
    }
    uint64_t total = (uint64_t)st.st_size;

    // cabecera
    hdr.name_len_be = htonl((uint32_t)strlen(name));
    hdr.total_be    = htonll_u64(total);
    if (sendn(d, &hdr, sizeof(hdr)) < 0) { perror("send hdr"); close(d); return 1; }
    if (sendn(d, name, strlen(name)) < 0) { perror("send name"); close(d); return 1; }

    // payload
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); close(d); return 1; }
    char *buf = (char*)malloc(BUF);
    uint64_t sent = 0;
    while (!feof(f)) {
      size_t rd = fread(buf, 1, BUF, f);
      if (rd == 0) break;
      if (sendn(d, buf, rd) < 0) { perror("send data"); free(buf); fclose(f); close(d); return 1; }
      sent += (uint64_t)rd;
    }
    free(buf); fclose(f);
    shutdown(d, SHUT_WR);

    // respuesta
    char ans[256]; int r = recv(d, ans, sizeof(ans)-1, 0);
    if (r > 0) { ans[r] = 0; log_line(ans); }
    close(d);
    return 0;
  }

  fprintf(stderr, "Operación desconocida: %s\n", op);
  close(d);
  return 1;
}

