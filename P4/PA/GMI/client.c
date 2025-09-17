/*
 * Cliente que realiza handshake en puerto de CONTROL y transfiere por puerto de DATOS:
 * - Paso 1 (CONTROL): conecta a <host>:<port_control>, envía OP(1) + ALEN(u32) + ALIAS,
 *                     recibe PORT_DATOS(u32).
 * - Paso 2 (DATOS):   conecta a <host>:PORT_DATOS y envía:
 *                     * STATUS:  ALEN(u32) | ALIAS
 *                     * UPLOAD:  ALEN(u32)|ALIAS|FLEN(u32)|FNAME|DLEN(u64)|DATA
 *   Respuesta de texto: "Servidor en espera" o "OK <bytes> <ruta>".
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ---------------- IO robusta ---------------- */

static ssize_t recvall(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = recv(fd, (char*)buf + off, len - off, 0);
        if (n == 0) return 0;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static ssize_t sendall(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, (const char*)buf + off, len - off, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

/* ---------------- Helpers framing ---------------- */

static int connect_tcp(const char *host, int port) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    int st = getaddrinfo(host, portstr, &hints, &res);
    if (st != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(st));
        return -1;
    }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { perror("socket"); freeaddrinfo(res); return -1; }

    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) { perror("connect"); close(s); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);
    return s;
}

static int send_u32(int fd, uint32_t val) {
    uint32_t net = htonl(val);
    return (int)(sendall(fd, &net, sizeof(net)) == sizeof(net));
}

static int recv_u32(int fd, uint32_t *out) {
    uint32_t net;
    if (recvall(fd, &net, sizeof(net)) != sizeof(net)) return 0;
    *out = ntohl(net);
    return 1;
}

static int send_u64(int fd, uint64_t val) {
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
    return (int)(sendall(fd, p, 8) == 8);
#else
    return (int)(sendall(fd, &val, 8) == 8);
#endif
}

/* ---------------- Log de respuesta ---------------- */

static void log_respuesta(const char *texto) {
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    FILE *f = fopen("cliente_log.txt", "a");
    if (f) {
        fprintf(f, "[%s] %s\n", ts, texto);
        fclose(f);
    }
    printf("[%s] Respuesta del servidor: %s\n", ts, texto);
}

/* ---------------- main ---------------- */

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  STATUS: %s <host/alias> <puerto_control> status\n", argv[0]);
        fprintf(stderr, "  UPLOAD: %s <host/alias> <puerto_control> upload <archivo>\n", argv[0]);
        return 1;
    }

    const char *host   = argv[1];
    int port_control   = atoi(argv[2]);
    const char *opstr  = argv[3];

    uint8_t op = 0xFF;
    if (strcmp(opstr, "status") == 0) op = 0;
    else if (strcmp(opstr, "upload") == 0) op = 1;

    if (op == 0xFF) {
        fprintf(stderr, "Operación no soportada: %s (usa: status|upload)\n", opstr);
        return 1;
    }

    // PASO 1: CONTROL
    int s_ctl = connect_tcp(host, port_control);
    if (s_ctl < 0) return 1;

    // Enviar: OP(1) + ALEN(u32) + ALIAS (usamos 'host' como alias lógico s01..s04)
    uint32_t alen = (uint32_t)strlen(host);
    if (sendall(s_ctl, &op, 1) != 1) { close(s_ctl); return 1; }
    if (!send_u32(s_ctl, alen))      { close(s_ctl); return 1; }
    if (sendall(s_ctl, host, alen) != (ssize_t)alen) { close(s_ctl); return 1; }

    // Recibir PORT_DATOS
    uint32_t port_data = 0;
    if (!recv_u32(s_ctl, &port_data) || port_data == 0 || port_data > 65535) {
        fprintf(stderr, "Puerto de datos inválido\n");
        close(s_ctl);
        return 1;
    }
    close(s_ctl);

    // PASO 2: DATOS
    int s_data = connect_tcp(host, (int)port_data);
    if (s_data < 0) return 1;

    if (op == 0) {
        // STATUS: ALEN|ALIAS
        if (!send_u32(s_data, alen)) { close(s_data); return 1; }
        if (sendall(s_data, host, alen) != (ssize_t)alen) { close(s_data); return 1; }

        char buf[512];
        ssize_t n = recv(s_data, buf, sizeof(buf) - 1, 0);
        if (n < 0) { perror("recv"); close(s_data); return 1; }
        buf[(n > 0) ? n : 0] = '\0';
        log_respuesta(buf);
    } else {
        // UPLOAD: ALEN|ALIAS|FLEN|FNAME|DLEN|DATA
        if (argc < 5) { fprintf(stderr, "Falta <archivo>\n"); close(s_data); return 1; }
        const char *fname = argv[4];
        FILE *fp = fopen(fname, "rb");
        if (!fp) { perror("fopen"); close(s_data); return 1; }

        // Calcular tamaño
        if (fseek(fp, 0, SEEK_END) != 0) { perror("fseek"); fclose(fp); close(s_data); return 1; }
        long fl = ftell(fp);
        if (fl < 0) { perror("ftell"); fclose(fp); close(s_data); return 1; }
        rewind(fp);

        uint32_t flen = (uint32_t)strlen(fname);
        uint64_t dlen = (uint64_t)fl;

        if (!send_u32(s_data, alen)) { fclose(fp); close(s_data); return 1; }
        if (sendall(s_data, host, alen) != (ssize_t)alen) { fclose(fp); close(s_data); return 1; }
        if (!send_u32(s_data, flen)) { fclose(fp); close(s_data); return 1; }
        if (sendall(s_data, fname, flen) != (ssize_t)flen) { fclose(fp); close(s_data); return 1; }
        if (!send_u64(s_data, dlen)) { fclose(fp); close(s_data); return 1; }

        // Enviar DATA en bloques
        char buf[8192];
        size_t nr;
        while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0) {
            if (sendall(s_data, buf, nr) != (ssize_t)nr) { perror("send"); fclose(fp); close(s_data); return 1; }
        }
        fclose(fp);

        // Respuesta
        char rbuf[1024];
        ssize_t n = recv(s_data, rbuf, sizeof(rbuf) - 1, 0);
        if (n < 0) { perror("recv"); close(s_data); return 1; }
        rbuf[(n > 0) ? n : 0] = '\0';
        log_respuesta(rbuf);
    }

    close(s_data);
    return 0;
}
