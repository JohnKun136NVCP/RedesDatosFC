/* P2/IGL/server.c
   Servidor TCP (puerto 7006) con función sysinfo():
   - Recibe "<clave_cifrada> <shift>"
   - Verifica en cipherworlds.txt (clave en minúsculas, sin espacios)
   - Si existe: descifra con César, envía "ACCESS GRANTED\n" y luego sysinfo.txt
   - Si no: envía "ACCESS DENIED\n"
*/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT   7006
#define BUFSZ  1024

/* --- utilidades pequeñas --- */
static void trim(char *s){
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}
static void to_lower(char *s){ for (; *s; ++s) *s = (char)tolower((unsigned char)*s); }

static void caesar_decrypt(char *t, int k){
    k %= 26;
    for (int i=0; t[i]; ++i){
        unsigned char c = (unsigned char)t[i];
        if (c >= 'A' && c <= 'Z')      t[i] = (char)('A' + (c - 'A' - k + 26) % 26);
        else if (c >= 'a' && c <= 'z') t[i] = (char)('a' + (c - 'a' - k + 26) % 26);
    }
}

/* --- validación de clave cifrada --- */
static bool key_in_file(const char *cipher){
    FILE *f = fopen("cipherworlds.txt", "r");
    if (!f){ perror("cipherworlds.txt"); return false; }
    char line[BUFSZ], target[BUFSZ];
    strncpy(target, cipher, sizeof(target)-1); target[sizeof(target)-1]='\0';
    trim(target); to_lower(target);
    bool ok = false;
    while (fgets(line, sizeof(line), f)){
        trim(line); to_lower(line);
        if (strcmp(line, target)==0){ ok = true; break; }
    }
    fclose(f);
    return ok;
}

/* --- helpers para sysinfo.txt --- */
static void run_and_append(FILE *fo, const char *title, const char *cmd){
    /* escribe encabezado y ejecuta cmd por /bin/sh -c, anexando su salida */
    fprintf(fo, "===== %s =====\n", title);
    FILE *pp = popen(cmd, "r");
    if (!pp){ fprintf(fo, "[Error al ejecutar: %s]\n\n", cmd); return; }
    char buf[BUFSZ]; size_t n;
    while ((n = fread(buf,1,sizeof(buf),pp)) > 0) fwrite(buf,1,n,fo);
    pclose(pp);
    fputc('\n', fo);
}

static void write_sysinfo(const char *path){
    FILE *fo = fopen(path, "w");
    if (!fo){ perror("sysinfo fopen"); return; }

    run_and_append(fo, "OS y Kernel",       "uname -s -r");
    run_and_append(fo, "Distribucion",
        "(. /etc/os-release 2>/dev/null && echo \"$PRETTY_NAME\") || "
        "lsb_release -ds 2>/dev/null || head -1 /etc/issue");
    run_and_append(fo, "IPs (IPv4/IPv6)",
        "ip -o addr show | awk '{print $2, $3, $4}'");
    run_and_append(fo, "CPU / Arquitectura",
        "lscpu 2>/dev/null | egrep 'Architecture|Model name|Vendor ID|CPU\\(s\\)|Thread|Core|Socket|MHz|Byte Order' || cat /proc/cpuinfo");
    run_and_append(fo, "Memoria (free -h)", "free -h");
    run_and_append(fo, "Disco (df -h)",     "df -h");
    run_and_append(fo, "Usuarios conectados","who");
    run_and_append(fo, "Todos los usuarios del sistema","cut -d: -f1 /etc/passwd");
    run_and_append(fo, "Uptime",            "uptime -p; echo; uptime");
    run_and_append(fo, "Procesos activos",
        "printf \"Total: \"; ps -e --no-headers | wc -l; echo; "
        "ps -eo pid,comm,pcpu,pmem --sort=-pcpu | head -n 15");
    run_and_append(fo, "Directorios montados",
        "command -v findmnt >/dev/null 2>&1 && findmnt -rno TARGET,SOURCE,FSTYPE,OPTIONS || mount");
    fclose(fo);
}

/* --- envío de archivos por socket --- */
static void send_file(int fd, const char *path){
    FILE *fi = fopen(path, "rb");
    if (!fi){ perror("send_file fopen"); return; }
    char buf[BUFSZ]; size_t n;
    while ((n = fread(buf,1,sizeof(buf),fi)) > 0){
        if (send(fd, buf, n, 0) < 0){ perror("send"); break; }
    }
    fclose(fi);
}

int main(void){
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0){ perror("socket"); return 1; }

    int yes=1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); close(sfd); return 1; }
    if (listen(sfd, 1) < 0){ perror("listen"); close(sfd); return 1; }
    printf("[+] Server escuchando en %d\n", PORT);

    struct sockaddr_in cli; socklen_t cl = sizeof(cli);
    int cfd = accept(sfd, (struct sockaddr*)&cli, &cl);
    if (cfd < 0){ perror("accept"); close(sfd); return 1; }
    printf("[+] Cliente conectado\n");

    char inbuf[BUFSZ]={0}, cipher[BUFSZ]={0}; int shift=0;
    int n = recv(cfd, inbuf, sizeof(inbuf)-1, 0);
    if (n <= 0){ puts("[-] No llegó dato"); close(cfd); close(sfd); return 1; }
    inbuf[n]='\0';

    if (sscanf(inbuf, "%1023s %d", cipher, &shift) != 2){
        const char *msg = "ACCESS DENIED\n";
        send(cfd, msg, strlen(msg), 0);
        puts("[-] Formato inválido");
        close(cfd); close(sfd); return 0;
    }
    printf("[srv] clave='%s' shift=%d\n", cipher, shift);

    if (key_in_file(cipher)){
        caesar_decrypt(cipher, shift);
        printf("[srv] descifrada='%s'\n", cipher);

        const char *ok = "ACCESS GRANTED\n";
        send(cfd, ok, strlen(ok), 0);

        write_sysinfo("sysinfo.txt");
        send_file(cfd, "sysinfo.txt");
        puts("[+] Enviado sysinfo.txt");
    } else {
        const char *no = "ACCESS DENIED\n";
        send(cfd, no, strlen(no), 0);
        puts("[-] Clave no encontrada");
    }

    close(cfd);
    close(sfd);
    return 0;
}
