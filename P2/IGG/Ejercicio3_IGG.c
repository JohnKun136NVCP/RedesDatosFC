#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006
#define BUFFER_SIZE 1024

/* --- Utilidades de texto --- */

static void toLowerCase(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static void trim(char *str) {
    char *p = str;
    while (isspace((unsigned char)*p)) p++;
    if (p != str) memmove(str, p, strlen(p)+1);
    size_t n = strlen(str);
    while (n>0 && isspace((unsigned char)str[n-1])) str[--n] = '\0';
}

/* --- César (descifrar = mover hacia atrás) --- */
static void decryptCaesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        if (isupper(c)) {
            text[i] = (char)('A' + ((c - 'A' - shift + 26) % 26));
        } else if (islower(c)) {
            text[i] = (char)('a' + ((c - 'a' - shift + 26) % 26));
        }
    }
}

/* Buscar exacto (case-insensitive y sin espacios extremos) en archivo de palabras EN CLARO */
static bool isOnFile(const char *bufferOriginal) {
    FILE *fp = fopen("cipherworlds.txt", "r");
    if (!fp) { perror("cipherworlds.txt"); return false; }

    char needle[BUFFER_SIZE];
    strncpy(needle, bufferOriginal, sizeof(needle)-1);
    needle[sizeof(needle)-1]='\0';
    trim(needle); toLowerCase(needle);

    char line[BUFFER_SIZE];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        trim(line); toLowerCase(line);
        if (strcmp(line, needle) == 0) { found = true; break; }
    }
    fclose(fp);
    return found;
}

/* Guardar info de red: se envía al cliente tras ACCESS GRANTED */
static void saveNetworkInfo(const char *outpath) {
    FILE *p = popen("ip addr show", "r");
    if (!p) { perror("popen"); return; }
    FILE *f = fopen(outpath, "w");
    if (!f) { perror("fopen"); pclose(p); return; }
    char buf[512];
    while (fgets(buf, sizeof(buf), p)) fputs(buf, f);
    fclose(f);
    pclose(p);
}

/* Enviar archivo por el socket */
static int sendFile(int sockfd, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return -1; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        ssize_t w = send(sockfd, buf, n, 0);
        if (w <= 0) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

int main(void) {
    /* 1) Crear socket servidor */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int opt = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;       /* Debian única VM -> 0.0.0.0 */
    addr.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(srv); return 1; }
    if (listen(srv, 1) < 0) { perror("listen"); close(srv); return 1; }
    printf("[+] Server listening on %d ...\n", PORT);

    /* 2) Aceptar un cliente y leer "<clave_cifrada> <shift>" (o "<clave> <shift>") */
    struct sockaddr_in cli; socklen_t clen = sizeof(cli);
    int c = accept(srv, (struct sockaddr*)&cli, &clen);
    if (c < 0) { perror("accept"); close(srv); return 1; }
    printf("[+] Client connected\n");

    char buffer[BUFFER_SIZE] = {0};
    ssize_t r = recv(c, buffer, sizeof(buffer)-1, 0);
    if (r <= 0) { fprintf(stderr, "[-] No key received\n"); close(c); close(srv); return 1; }
    buffer[r] = '\0';

    /* Parseo robusto: exige dos campos: clave y shift */
    char key_in[BUFFER_SIZE] = {0};
    int shift = 0;
    if (sscanf(buffer, "%1023s %d", key_in, &shift) != 2) {
        const char *fmt = "ACCESS DENIED\n";
        send(c, fmt, strlen(fmt), 0);
        fprintf(stderr, "[-] Invalid format. Expected: <key> <shift>\n");
        close(c); close(srv); return 1;
    }

    decryptCaesar(key_in, shift);
    toLowerCase(key_in); trim(key_in);

    if (isOnFile(key_in)) {
        const char *ok = "ACCESS GRANTED\n";
        send(c, ok, strlen(ok), 0);
        saveNetworkInfo("network_info.txt");
        (void)sendFile(c, "network_info.txt");
        printf("[+] Sent network_info.txt\n");
    } else {
        const char *no = "ACCESS DENIED\n";
        send(c, no, strlen(no), 0);
        printf("[-] Wrong key after decrypt\n");
    }

    close(c);
    close(srv);
    return 0;
}
