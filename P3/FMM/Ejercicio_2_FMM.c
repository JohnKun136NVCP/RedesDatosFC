// Client.c
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _attribute_((packed)) Header {
    uint32_t target_port_be; // puerto objetivo
    int32_t  shift_be;       // desplazamiento César
    uint32_t data_len_be;    // longitud del texto
};

static ssize_t sendall(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, p + sent, len - sent, 0);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}

static ssize_t recvall(int fd, void *buf, size_t len) {
    unsigned char *p = (unsigned char *)buf;
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, p + got, len - got, 0);
        if (r == 0) {
            return got; // peer closed
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        got += (size_t)r;
    }
    return (ssize_t)got;
}

static int connect_to(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        perror("inet_pton");
        close(s);
        return -1;
    }

    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s <SERVIDOR_IP> <PUERTO_OBJETIVO> <SHIFT> <archivo.txt>\n",
                argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int target_port = atoi(argv[2]);
    int shift       = atoi(argv[3]);
    const char *path = argv[4];

    // Leer archivo completo
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    long L = ftell(fp);
    if (L < 0) {
        perror("ftell");
        fclose(fp);
        return 1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    char *buf = (char *)malloc((size_t)L + 1);
    if (buf == NULL) {
        fprintf(stderr, "malloc\n");
        fclose(fp);
        return 1;
    }

    size_t rd = fread(buf, 1, (size_t)L, fp);
    if (rd != (size_t)L) {
        perror("fread");
        free(buf);
        fclose(fp);
        return 1;
    }
    buf[L] = '\0';
    fclose(fp);

    // Lista de puertos del servidor
    int server_ports[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; ++i) {
        int p = server_ports[i];

        int s = connect_to(server_ip, p);
        if (s < 0) {
            // Si no hay conexión, lo consideramos como rechazado
            printf("[*] SERVER RESPONSE %d: REJECTED\n", p);
            continue;
        }

        struct Header h;
        h.target_port_be = htonl((uint32_t)target_port);
        h.shift_be       = htonl(shift);
        h.data_len_be    = htonl((uint32_t)L);

        if (sendall(s, &h, sizeof(h)) < 0) {
            printf("[*] SERVER RESPONSE %d: REJECTED\n", p);
            close(s);
            continue;
        }

        if (sendall(s, buf, (size_t)L) < 0) {
            printf("[*] SERVER RESPONSE %d: REJECTED\n", p);
            close(s);
            continue;
        }

        uint32_t resp_be = 0;
        uint32_t resp = 0;

        if (recvall(s, &resp_be, sizeof(resp_be)) == (ssize_t)sizeof(resp_be)) {
            resp = ntohl(resp_be);
        }
        else {
            // Si no recibimos respuesta completa, lo consideramos rechazado
            resp = 0;
        }

        if (resp == 1) {
            printf("[*] SERVER RESPONSE %d: File received and encrypted.\n", p);
        }
        else {
            printf("[*] SERVER RESPONSE %d: REJECTED\n", p);
        }

        close(s);
    }

    free(buf);
    return 0;
}
