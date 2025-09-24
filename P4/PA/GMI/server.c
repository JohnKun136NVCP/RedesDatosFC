/*

 * Servidor multipuerto con "puerto de control" + "puerto de datos" por conexión.
 * - Escucha en un puerto de control (por defecto 49200).
 * - Cada solicitud del cliente recibe: OP (1 byte) + ALEN (uint32) + ALIAS.
 * - Responde con PORT_DATOS (uint32, network byte order).
 * - Abre un socket de datos efímero (bind a puerto 0), hace listen y accept.
 * - En el socket de datos:
 *      Si OP=0 (status): recibe ALEN+ALIAS y responde un texto de estado.
 *      Si OP=1 (upload): recibe un frame binario:
 *           ALEN (u32) | ALIAS | FLEN (u32) | FNAME | DLEN (u64) | DATA
 *        Valida alias contra argv[], guarda DATA como $HOME/<alias>/<fname>,
 *        responde "OK <bytes> <ruta>".
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ---------------- Utilidades de logging ---------------- */

static void log_ts(const char *fmt, ...) {
    char ts[64];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stdout, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
}

/* ---------------- IO robusta (recvall / sendall) ---------------- */

static ssize_t recvall(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = recv(fd, (char*)buf + off, len - off, 0);
        if (n == 0) return 0;             // peer closed
        if (n < 0) {
            if (errno == EINTR) continue; // retry
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static ssize_t sendall(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, (const char*)buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue; // retry
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

/* ---------------- Helpers de framing ---------------- */

static bool recv_u32(int fd, uint32_t *out) {
    uint32_t net;
    if (recvall(fd, &net, sizeof(net)) != sizeof(net)) return false;
    *out = ntohl(net);
    return true;
}

static bool send_u32(int fd, uint32_t val) {
    uint32_t net = htonl(val);
    return sendall(fd, &net, sizeof(net)) == sizeof(net);
}

static bool recv_u64(int fd, uint64_t *out) {
    uint64_t net;
    if (recvall(fd, &net, sizeof(net)) != sizeof(net)) return false;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    // no hay ntohll estándar portable; convertimos manualmente
    uint8_t *p = (uint8_t*)&net;
    uint64_t v =
        ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
        ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
        ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
        ((uint64_t)p[6] <<  8) | ((uint64_t)p[7] <<  0);
    *out = v;
#else
    *out = net;
#endif
    return true;
}

static bool send_u64(int fd, uint64_t val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t p[8] = {
        (uint8_t)((val >> 56) & 0xFF),
        (uint8_t)((val >> 48) & 0xFF),
        (uint8_t)((val >> 40) & 0xFF),
        (uint8_t)((val >> 32) & 0xFF),
        (uint8_t)((val >> 24) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >>  8) & 0xFF),
        (uint8_t)((val >>  0) & 0xFF)
    };
    return sendall(fd, p, 8) == 8;
#else
    return sendall(fd, &val, 8) == 8;
#endif
}

/* ---------------- Validación de alias y rutas ---------------- */

static bool alias_permitido(const char *alias, int argc, char **argv) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(alias, argv[i]) == 0) return true;
    }
    return false;
}

static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, 0700) == 0) return true;
    return false;
}

static bool path_guardar_archivo(char *dst, size_t dstsz, const char *alias, const char *fname) {
    const char *home = getenv("HOME");
    if (!home || !*home) home = ".";
    // $HOME/<alias>/<fname>
    if (snprintf(dst, dstsz, "%s/%s", home, alias) >= (int)dstsz) return false;
    if (!ensure_dir(dst)) return false;
    if (snprintf(dst, dstsz, "%s/%s/%s", home, alias, fname) >= (int)dstsz) return false;
    return true;
}

/* ---------------- Socket de datos efímero ---------------- */

static int listen_port_ephemeral(uint16_t *out_port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(0); // efímero

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s); return -1;
    }
    if (listen(s, 8) < 0) {
        close(s); return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(s, (struct sockaddr*)&addr, &len) < 0) {
        close(s); return -1;
    }
    *out_port = ntohs(addr.sin_port);
    return s;
}

/* ---------------- Manejo de operación STATUS ---------------- */

static void handle_status(int data_fd, const char *alias) {
    (void)alias; // En una versión extendida, podríamos reportar métricas por alias.
    const char *msg = "Servidor en espera"; // Mensaje alineado a tu práctica previa/actual. :contentReference[oaicite:2]{index=2}
    sendall(data_fd, msg, strlen(msg));
}

/* ---------------- Manejo de operación UPLOAD ---------------- */

static void handle_upload(int data_fd, const char *alias, int argc, char **argv) {
    // Recibe: ALEN|ALIAS|FLEN|FNAME|DLEN|DATA  (ya recibimos OP en el socket de control)
    uint32_t alen=0, flen=0;
    uint64_t dlen=0;

    // ALIAS (se vuelve a enviar para autoconsistencia del frame de datos)
    if (!recv_u32(data_fd, &alen)) return;
    if (alen == 0 || alen > 4096) return;
    char *alias2 = (char*)malloc(alen+1);
    if (!alias2) return;
    if (recvall(data_fd, alias2, alen) != (ssize_t)alen) { free(alias2); return; }
    alias2[alen] = '\0';

    // Nombre de archivo
    if (!recv_u32(data_fd, &flen)) { free(alias2); return; }
    if (flen == 0 || flen > 4096) { free(alias2); return; }
    char *fname = (char*)malloc(flen+1);
    if (!fname) { free(alias2); return; }
    if (recvall(data_fd, fname, flen) != (ssize_t)flen) { free(alias2); free(fname); return; }
    fname[flen] = '\0';

    // Longitud de datos y cuerpo
    if (!recv_u64(data_fd, &dlen)) { free(alias2); free(fname); return; }

    // Validación de alias (según argv[] pasados al servidor). :contentReference[oaicite:3]{index=3}
    if (!alias_permitido(alias2, argc, argv)) {
        const char *err = "Alias no permitido";
        sendall(data_fd, err, strlen(err));
        free(alias2); free(fname);
        // Consumir el cuerpo si el cliente igualmente lo envía (evitar RST)
        char dropbuf[4096];
        uint64_t left = dlen;
        while (left > 0) {
            ssize_t r = recv(data_fd, dropbuf, (left > sizeof(dropbuf)) ? sizeof(dropbuf) : (size_t)left, 0);
            if (r <= 0) break;
            left -= (uint64_t)r;
        }
        return;
    }

    char fullpath[PATH_MAX];
    if (!path_guardar_archivo(fullpath, sizeof(fullpath), alias2, fname)) {
        const char *err = "No se pudo preparar ruta de guardado";
        sendall(data_fd, err, strlen(err));
        free(alias2); free(fname);
        // Consumir DATA
        char dropbuf[4096]; uint64_t left = dlen;
        while (left > 0) { ssize_t r = recv(data_fd, dropbuf, (left > sizeof(dropbuf)) ? sizeof(dropbuf) : (size_t)left, 0); if (r <= 0) break; left -= (uint64_t)r; }
        return;
    }

    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        const char *err = "No se pudo crear el archivo destino";
        sendall(data_fd, err, strlen(err));
        free(alias2); free(fname);
        // Consumir DATA
        char dropbuf[4096]; uint64_t left = dlen;
        while (left > 0) { ssize_t r = recv(data_fd, dropbuf, (left > sizeof(dropbuf)) ? sizeof(dropbuf) : (size_t)left, 0); if (r <= 0) break; left -= (uint64_t)r; }
        return;
    }

    uint64_t recibidos = 0;
    char buf[8192];
    while (recibidos < dlen) {
        size_t want = (size_t)((dlen - recibidos) > sizeof(buf) ? sizeof(buf) : (dlen - recibidos));
        ssize_t n = recv(data_fd, buf, want, 0);
        if (n <= 0) break;
        fwrite(buf, 1, (size_t)n, fp);
        recibidos += (uint64_t)n;
    }
    fclose(fp);

    log_ts("Archivo recibido: alias=%s, nombre=%s, bytes=%llu, ruta=%s",
           alias2, fname, (unsigned long long)recibidos, fullpath);

    char okmsg[1024];
    snprintf(okmsg, sizeof(okmsg), "OK %llu %s", (unsigned long long)recibidos, fullpath);
    sendall(data_fd, okmsg, strlen(okmsg));

    free(alias2);
    free(fname);
}

/* ---------------- main (loop de control) ---------------- */

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <puerto_control> <alias1> [alias2] [...]\n", argv[0]);
        fprintf(stderr, "Ej.: %s 49200 s01 s02 s03 s04\n", argv[0]);
        return 1;
    }

    int control_port = atoi(argv[1]);
    if (control_port <= 0 || control_port > 65535) {
        fprintf(stderr, "Puerto de control inválido\n");
        return 1;
    }

    int s_ctl = socket(AF_INET, SOCK_STREAM, 0);
    if (s_ctl < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(s_ctl, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)control_port);

    if (bind(s_ctl, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(s_ctl); return 1; }
    if (listen(s_ctl, 16) < 0) { perror("listen"); close(s_ctl); return 1; }

    log_ts("Servidor de CONTROL escuchando en %d ...", control_port);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cfd = accept(s_ctl, (struct sockaddr*)&cli, &clen);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        // 1) Recibe OP (1 byte) + ALEN (u32) + ALIAS
        uint8_t op;
        if (recvall(cfd, &op, 1) != 1) { close(cfd); continue; }

        uint32_t alen = 0;
        if (!recv_u32(cfd, &alen) || alen == 0 || alen > 4096) { close(cfd); continue; }

        char *alias = (char*)malloc(alen + 1);
        if (!alias) { close(cfd); continue; }
        if (recvall(cfd, alias, alen) != (ssize_t)alen) { free(alias); close(cfd); continue; }
        alias[alen] = '\0';

        // 2) Abrir socket de DATOS efímero y enviar el puerto al cliente
        uint16_t data_port = 0;
        int s_data = listen_port_ephemeral(&data_port);
        if (s_data < 0) {
            free(alias);
            close(cfd);
            continue;
        }
        if (!send_u32(cfd, (uint32_t)data_port)) {
            free(alias);
            close(s_data);
            close(cfd);
            continue;
        }

        log_ts("Handshake: OP=%u, alias=%s -> puerto de DATOS=%u", (unsigned)op, alias, (unsigned)data_port);

        // 3) Aceptar conexión de DATOS y atender la operación
        struct sockaddr_in cli2;
        socklen_t cli2len = sizeof(cli2);
        int dfd = accept(s_data, (struct sockaddr*)&cli2, &cli2len);
        close(s_data); // ya no necesitamos escuchar más en este puerto

        if (dfd < 0) {
            free(alias);
            close(cfd);
            continue;
        }

        if (op == 0) {
            // STATUS: el cliente reenvía ALEN|ALIAS (consistencia de frame)
            uint32_t al2 = 0;
            if (!recv_u32(dfd, &al2) || al2 == 0 || al2 > 4096) {
                // cierre limpio
            } else {
                char *alias2 = (char*)malloc(al2 + 1);
                if (alias2 && recvall(dfd, alias2, al2) == (ssize_t)al2) {
                    alias2[al2] = '\0';
                    handle_status(dfd, alias2);
                }
                free(alias2);
            }
        } else if (op == 1) {
            // UPLOAD
            handle_upload(dfd, alias, argc, argv);
        } else {
            const char *err = "OP no soportada";
            sendall(dfd, err, strlen(err));
        }

        close(dfd);
        free(alias);
        close(cfd);
    }

    close(s_ctl);
    return 0;
}
