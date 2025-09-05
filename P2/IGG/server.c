/* P2 | Servidor TCP | Clave: wpvkpi | keys.txt: "28 jcwpvkpi" | IGG */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PORT 7006
#define BUFSZ 1024

static void tolower_inplace(char *s){
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static void caesar_encrypt(char *dst, const char *src, int shift){
    int k = ((shift % 26) + 26) % 26;
    for (size_t i = 0; src[i]; ++i){
        unsigned char c = (unsigned char)src[i];
        if (isalpha(c)){
            int base = islower(c) ? 'a' : 'A';
            dst[i] = (char)(base + ((c - base + k) % 26));
        } else {
            dst[i] = (char)c;
        }
    }
}

static void save_network_info(const char *outpath){
    FILE *p = popen("ip addr show", "r");
    if(!p){ perror("popen"); return; }
    FILE *f = fopen(outpath, "w");
    if(!f){ perror("fopen"); pclose(p); return; }
    char buf[1024];
    while (fgets(buf, sizeof(buf), p)) fputs(buf, f);
    fclose(f);
    pclose(p);
}

static int send_file(int sock, const char *path){
    FILE *f = fopen(path, "rb");
    if(!f){ perror("fopen"); return -1; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0){
        ssize_t w = send(sock, buf, n, 0);
        if (w <= 0){ fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

int main(void){
    /* 1) Cargar shift y claveCifrada de keys.txt (formato: "<shift> <cipher>") */
    int shift_in_file = 0;
    char cipher_in_file[BUFSZ] = {0};
    {
        FILE *kf = fopen("keys.txt","r");
        if(!kf){ fprintf(stderr,"[-] No se pudo abrir keys.txt\n"); return 1; }
        if (fscanf(kf, "%d %1023s", &shift_in_file, cipher_in_file) != 2){
            fprintf(stderr,"[-] Formato inválido en keys.txt (esperado: \"<shift> <cipher>\")\n");
            fclose(kf); return 1;
        }
        fclose(kf);
        tolower_inplace(cipher_in_file);
    }

    /* 2) Preparar socket servidor */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if(srv < 0){ perror("socket"); return 1; }
    int opt=1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if(bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); close(srv); return 1; }
    if(listen(srv, 1) < 0){ perror("listen"); close(srv); return 1; }
    printf("[+] Server escuchando en %d ...\n", PORT);

    /* 3) Aceptar un cliente y recibir "clave shift" en texto */
    struct sockaddr_in cli; socklen_t clen = sizeof(cli);
    int c = accept(srv, (struct sockaddr*)&cli, &clen);
    if(c < 0){ perror("accept"); close(srv); return 1; }
    printf("[+] Cliente conectado\n");

    char line[BUFSZ] = {0};
    ssize_t r = recv(c, line, sizeof(line)-1, 0);
    if(r <= 0){ fprintf(stderr,"[-] Sin datos\n"); close(c); close(srv); return 1; }
    line[r] = 0;

    char key_plain[BUFSZ] = {0};
    int shift_from_client = 0;
    if (sscanf(line, "%1023s %d", key_plain, &shift_from_client) != 2){
        fprintf(stderr,"[-] Formato recibido inválido. Esperado: \"<clave> <shift>\".\n");
        close(c); close(srv); return 1;
    }
    tolower_inplace(key_plain);

    /* 4) Validar contra keys.txt:
          - El shift debe coincidir con el shift del archivo
          - Al cifrar la clave en claro con ese shift debe dar el cipher del archivo */
    char key_enc[BUFSZ] = {0};
    caesar_encrypt(key_enc, key_plain, shift_from_client);
    tolower_inplace(key_enc);

    if (shift_from_client == shift_in_file && strcmp(key_enc, cipher_in_file) == 0){
        const char *ok = "ACCESS GRANTED\n";
        send(c, ok, strlen(ok), 0);
        save_network_info("network_info.txt");
        (void)send_file(c, "network_info.txt");
        printf("[+] Enviada network_info.txt\n");
    } else {
        const char *no = "ACCESS DENIED\n";
        send(c, no, strlen(no), 0);
        printf("[-] Clave/shift no coinciden con keys.txt\n");
    }

    close(c);
    close(srv);
    return 0;
}
