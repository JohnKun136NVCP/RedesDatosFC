// cliente.c – Práctica 3 Redes
// ./cliente <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <archivo>
// me conecto a los 3 servidores (49200, 49201, 49202)
// Solo el servidor cuyo puerto coincide con PORTO debe procesar

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define BUFFER_SIZE 1024

// lee un archivo y se queda solo con letras 
static char *leerSoloLetras(const char *ruta, size_t *tamFinal) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror("no pude abrir el archivo :()"); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *raw = malloc(sz + 1);
    if (!raw) { fclose(f); return NULL; }
    size_t n = fread(raw, 1, sz, f);
    fclose(f);
    raw[n] = '\0';

    // armo el limpio
    char *clean = malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)raw[i];
        // paso vocales con acento y ñ a su versión normal
        if (c == 0xC3 && i + 1 < n) {
            unsigned char d = (unsigned char)raw[i+1];
            if (d==0xA1||d==0x81) { c='a'; i++; }
            else if (d==0xA9||d==0x89) { c='e'; i++; }
            else if (d==0xAD||d==0x8D) { c='i'; i++; }
            else if (d==0xB3||d==0x93) { c='o'; i++; }
            else if (d==0xBA||d==0x9A) { c='u'; i++; }
            else if (d==0xB1||d==0x91) { c='n'; i++; }
        }
        if (isalpha(c)) clean[j++] = (char)c;
    }
    clean[j] = '\0';
    free(raw);
    *tamFinal = j;
    return clean;
}

// Conecto a un puerto y muestro lo que conteste
static int mandar(const char *ip, int puertoServer,
                  int portoObjetivo, int shift,
                  const char *msg, size_t len) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)puertoServer);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        perror("inet_pton"); close(s); return -1;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(s); return -1;
    }

    // cabecera
    char header[128];
    int hlen = snprintf(header, sizeof(header),
                        "PORTO=%d;SHIFT=%d;LEN=%zu\n",
                        portoObjetivo, shift, len);
    if (send(s, header, hlen, 0) != hlen) { perror("send header"); close(s); return -1; }

    // cuerpo
    size_t sent = 0;
    while (sent < len) {
        ssize_t w = send(s, msg + sent, len - sent, 0);
        if (w <= 0) { perror("send body"); close(s); return -1; }
        sent += (size_t)w;
    }

    printf("\n--- Respuesta de %s:%d (PORTO=%d) ---\n", ip, puertoServer, portoObjetivo);
    char buf[BUFFER_SIZE+1];
    for (;;) {
        ssize_t r = recv(s, buf, BUFFER_SIZE, 0);
        if (r <= 0) break;
        buf[r] = '\0';
        fputs(buf, stdout);
    }
    printf("\n");
    close(s);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <archivo>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int portoObjetivo = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *archivo = argv[4];

    // limpio el archivo
    size_t len = 0;
    char *msg = leerSoloLetras(archivo, &len);
    if (!msg || len == 0) {
        fprintf(stderr, "El archivo no se pudo leer.\n");
        free(msg);
        return 1;
    }

    // los 3 puertos fijos 
    int servers[3] = {49200, 49201, 49202};
    for (int i = 0; i < 3; i++) {
        mandar(ip, servers[i], portoObjetivo, shift, msg, len);
    }

    free(msg);
    return 0;
}
//finnnn