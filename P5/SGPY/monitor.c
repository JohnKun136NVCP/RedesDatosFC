// monitor.c
// Compila: gcc -O2 -Wall monitor.c -o monitor
// Ejecuta: ./monitor [puerto]     (por defecto 49300)

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_MON_PORT 49300
#define MAXS 16
#define LINE_MAX 512
#define TIME_SLICE_MS 1000   
#define DONE_TIMEOUT_MS 0    

struct Server {
    int sock;
    char id[32];
    int registered;
};

static struct Server S[MAXS];
static int NS = 0;

static void die(const char* m) { perror(m); exit(1); }

static int bind_listen_ipv4(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) die("socket");
    int yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) die("setsockopt");
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) die("bind");
    if (listen(s, 16) < 0) die("listen");
    return s;
}

static ssize_t read_line(int s, char* buf, size_t n, int timeout_ms) {
    
    size_t i = 0;
    while (i + 1 < n) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
        struct timeval tv, * ptv = NULL;
        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            ptv = &tv;
        }
        int r = select(s + 1, &rfds, NULL, NULL, ptv);
        if (r == 0) return 0;      // timeout
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        char c;
        ssize_t k = recv(s, &c, 1, 0);
        if (k <= 0) return k;      // cierre o error
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0'; return (ssize_t)i;
}

static int send_line(int s, const char* line) {
    size_t n = strlen(line);
    ssize_t w = send(s, line, n, 0);
    if (w < 0) return -1;
    
    if (n == 0 || line[n - 1] != '\n') {
        if (send(s, "\n", 1, 0) < 0) return -1;
    }
    return 0;
}

static void broadcast(const char* msg) {
    for (int i = 0; i < NS; i++) {
        if (S[i].registered && S[i].sock >= 0) {
            send_line(S[i].sock, msg);
        }
    }
}

static int accept_and_register(int ls) {
    struct sockaddr_in c; socklen_t cl = sizeof(c);
    int cs = accept(ls, (struct sockaddr*)&c, &cl);
    if (cs < 0) return -1;

    char line[LINE_MAX] = { 0 };
    ssize_t n = read_line(cs, line, sizeof(line), 5000); 
    if (n <= 0) {
        close(cs);
        return -1;
    }

    char cmd[16] = { 0 }, id[32] = { 0 };
    if (sscanf(line, "%15s %31s", cmd, id) != 2 || strcasecmp(cmd, "HELLO") != 0) {
        // protocolo inválido
        send_line(cs, "ERR expected HELLO <id>");
        close(cs);
        return -1;
    }

    if (NS >= MAXS) {
        send_line(cs, "ERR too many servers");
        close(cs);
        return -1;
    }

    // registra
    S[NS].sock = cs;
    strncpy(S[NS].id, id, sizeof(S[NS].id) - 1);
    S[NS].registered = 1;
    NS++;

    // Envía OK y broadcast de quién se conectó (informativo)
    send_line(cs, "OK");
    char msg[64]; snprintf(msg, sizeof(msg), "JOIN %s", id);
    broadcast(msg);

    fprintf(stdout, "[monitor] registrado: %s (sock=%d)\n", id, cs);
    return 0;
}

static int wait_for_reply(int sock, char* out, size_t out_sz, int timeout_ms) {
    // Espera una línea (START/SKIP/DONE/...) con timeout_ms
    ssize_t n = read_line(sock, out, out_sz, timeout_ms);
    if (n <= 0) return (int)n; // 0=timeout, -1=error/conexion cerrada
    return 1;
}

int main(int argc, char** argv) {
    int mon_port = (argc >= 2) ? atoi(argv[1]) : DEFAULT_MON_PORT;

    int ls = bind_listen_ipv4(mon_port);
    printf("[monitor] escuchando en %d\n", mon_port);

    
    while (NS == 0) {
        if (accept_and_register(ls) < 0) {
            // nada
        }
    }
    printf("[monitor] servidores registrados iniciales: %d\n", NS);
    printf("[monitor] comenzando round-robin…\n");

    int turn = 0;
    for (;;) {
        
        fd_set rf; FD_ZERO(&rf); FD_SET(ls, &rf);
        struct timeval tv0 = { 0,0 };
        if (select(ls + 1, &rf, NULL, NULL, &tv0) > 0) {
            accept_and_register(ls); // registra extra si llega
        }

        
        int tries = 0;
        while ((!S[turn].registered || S[turn].sock < 0) && tries < NS) {
            turn = (turn + 1) % NS;
            tries++;
        }
        if (!S[turn].registered || S[turn].sock < 0) {
            
            usleep(50 * 1000);
            continue;
        }

       
        char turnMsg[64]; snprintf(turnMsg, sizeof(turnMsg), "TURN %s", S[turn].id);
        if (send_line(S[turn].sock, turnMsg) < 0) {
            
            fprintf(stderr, "[monitor] %s desconectado al enviar TURN\n", S[turn].id);
            S[turn].registered = 0; close(S[turn].sock); S[turn].sock = -1;
            turn = (turn + 1) % NS;
            continue;
        }

        
        char reply[LINE_MAX] = { 0 };
        int r = wait_for_reply(S[turn].sock, reply, sizeof(reply), TIME_SLICE_MS);

        if (r == 0) {
            
            turn = (turn + 1) % NS;
            continue;
        }
        else if (r < 0) {
            
            fprintf(stderr, "[monitor] %s desconectado (sin respuesta)\n", S[turn].id);
            S[turn].registered = 0; close(S[turn].sock); S[turn].sock = -1;
            turn = (turn + 1) % NS;
            continue;
        }

        
        if (strncasecmp(reply, "START", 5) == 0) {
            
            char busy[64]; snprintf(busy, sizeof(busy), "BUSY %s", S[turn].id);
            broadcast(busy);

            
            int wait_ms = DONE_TIMEOUT_MS;
            for (;;) {
                int rr = wait_for_reply(S[turn].sock, reply, sizeof(reply), (wait_ms == 0 ? -1 : wait_ms));
                if (rr <= 0) {
                    
                    fprintf(stderr, "[monitor] %s desconectado/timeout esperando DONE\n", S[turn].id);
                    S[turn].registered = 0; close(S[turn].sock); S[turn].sock = -1;
                    break;
                }
                if (strncasecmp(reply, "DONE", 4) == 0) {
                    
                    broadcast("IDLE");
                    
                    break;
                }
                
            }
            turn = (turn + 1) % NS;
            continue;
        }
        else if (strncasecmp(reply, "SKIP", 4) == 0) {
            
            turn = (turn + 1) % NS;
            continue;
        }
        else {
            
            turn = (turn + 1) % NS;
            continue;
        }
    }

   
    close(ls);
    return 0;
}
