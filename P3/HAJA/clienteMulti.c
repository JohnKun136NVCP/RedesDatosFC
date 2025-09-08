// clientMulti.c – Práctica 3
// ./clientMulti <IP> <SHIFT> 49200:msg1.txt 49201:msg2.txt ...
// Por cada PUERTO:ARCHIVO me conecto a 49200, 49201 y 49202.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Leo el archivo y me quedo solo con letras 
static char *leer_y_limpiar(const char *path, size_t *out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("no pude abrir el archivo :( )"); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *raw = malloc((size_t)sz + 1);
    if (!raw) { fclose(f); return NULL; }

    size_t n = fread(raw, 1, (size_t)sz, f);
    fclose(f);
    raw[n] = '\0';

    // solo letras convierto vocales acentuadas 
    char *clean = malloc(n + 1);
    if (!clean) { free(raw); return NULL; }

    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)raw[i];

        // Manejo para acentos 
        if (c == 0xC3 && i + 1 < n) {
            unsigned char d = (unsigned char)raw[i+1];
            if (d==0xA1||d==0x81) { c='a'; i++; }  // á Á
            else if (d==0xA9||d==0x89) { c='e'; i++; }  // é É
            else if (d==0xAD||d==0x8D) { c='i'; i++; }  // í Í
            else if (d==0xB3||d==0x93) { c='o'; i++; }  // ó Ó
            else if (d==0xBA||d==0x9A) { c='u'; i++; }  // ú Ú
            else if (d==0xB1||d==0x91) { c='n'; i++; }  // ñ Ñ
        }

        if (isalpha(c)) clean[j++] = (char)c;
    }
    clean[j] = '\0';

    free(raw);
    *out = j;
    return clean;
}

// Me conecto a un server y muestro lo que responda
static int manda(const char *ip, int srv_port,
                 int porto_obj, int shift,
                 const char *msg, size_t len) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)srv_port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        perror("inet_pton");
        close(s);
        return -1;
    }

    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("connect");
        close(s);
        return -1;
    }

    // Cabecera 
    char header[128];
    int h = snprintf(header, sizeof(header),
                     "PORTO=%d;SHIFT=%d;LEN=%zu\n",
                     porto_obj, shift, len);
    if (h <= 0 || h >= (int)sizeof(header)) {
        fprintf(stderr, "header inválido\n");
        close(s);
        return -1;
    }

    // Envío cabecera
    if (send(s, header, (size_t)h, 0) != h) {
        perror("send header");
        close(s);
        return -1;
    }

    // Envío cuerpo 
    size_t sent = 0;
    while (sent < len) {
        ssize_t w = send(s, msg + sent, len - sent, 0);
        if (w <= 0) { perror("send body"); close(s); return -1; }
        sent += (size_t)w;
    }

    // Muestro lo que responda el server
    char buf[2048 + 1];
    printf("---- Respuesta %s:%d (PORTO=%d) ----\n", ip, srv_port, porto_obj);
    for (;;) {
        ssize_t r = recv(s, buf, 2048, 0);
        if (r < 0) { perror("recv"); break; }
        if (r == 0) break;
        buf[r] = '\0';
        fputs(buf, stdout);
    }
    printf("\n");

    close(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <IP> <SHIFT> <PUERTO:ARCHIVO>...\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int shift = atoi(argv[2]);

    // Los 3 puertos fijos 
    int servidores[3] = {49200, 49201, 49202};

    // Recorro cada argumento PUERTO:ARCHIVO
    for (int i = 3; i < argc; i++) {
        char *par = argv[i];
        char *col = strchr(par, ':');
        if (!col) {
            fprintf(stderr, "Arg inválido: %s (usa PUERTO:ARCHIVO)\n", par);
            continue;
        }

        *col = '\0';
        int porto = atoi(par);
        const char *ruta = col + 1;

        size_t n = 0;
        char *msg = leer_y_limpiar(ruta, &n);
        if (!msg) {
            fprintf(stderr, "No pude leer %s\n", ruta);
            *col = ':'; 
            continue;
        }
        if (n == 0) {
            fprintf(stderr, "Archivo vacío: %s\n", ruta);
            free(msg);
            *col = ':';
            continue;
        }

        printf("== Enviando %s (PORTO=%d, SHIFT=%d) ==\n", ruta, porto, shift);

        // Por cada archivo me conecto a los tres servers, este es el punto
        for (int k = 0; k < 3; k++) {
            if (manda(ip, servidores[k], porto, shift, msg, n) != 0) {
                fprintf(stderr, "falló con %s:%d\n", ip, servidores[k]);
            }
        }

        free(msg);
        *col = ':'; 
    }

    return 0;
}

//finnnn
