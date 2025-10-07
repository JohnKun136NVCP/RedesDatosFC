// Ejercicio_3_BGA.c — Practica II (este es el codigo de server.c pero se le cambio el nombre por la entrega en git, dejo ambas formas de compilarlo por si es necesario, solo seria cambiarle el nombre al archivo)
// Compilar: gcc Ejercicio_3_BGA.c -o Ejercicio_3_BGA
// Ejecutar: ./Ejercicio_3_BGA

// o (cambiandole el nombre)

// Compilar: gcc server.c -o server
// Ejecutar: ./server

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#define PORT 7006
#define BUFFER_SIZE 1024

static void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) str[i] = (char)tolower((unsigned char)str[i]);
}

static void trim(char *str) {
    char *start = str, *end;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);
    end = str + strlen(str);
    while (end > str && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
}

static void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' - shift + 26) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' - shift + 26) % 26) + 'a');
        } else {
            text[i] = c;
        }
    }
}

// Verificar clave en cipherworlds.txt
static bool isOnFile(const char *bufferOriginal) {
    FILE *fp = fopen("cipherworlds.txt", "r");
    if (!fp) {
        fprintf(stderr, "[-] Error abriendo cipherworlds.txt\n");
        return false;
    }
    char line[BUFFER_SIZE], buf[BUFFER_SIZE];
    strncpy(buf, bufferOriginal, BUFFER_SIZE - 1);
    buf[BUFFER_SIZE - 1] = '\0';
    trim(buf);
    toLowerCase(buf);

    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buf) == 0) { found = true; break; }
    }
    fclose(fp);
    return found;
}

// Enviar archivo por el socket
static void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("[-] No se pudo abrir el archivo a enviar");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error al enviar el archivo");
            break;
        }
    }
    fclose(fp);
}

// Helpers para sysinfo
static void write_line(FILE *f, const char *title) {
    fprintf(f, "\n %s \n", title);
}

static int dump_cmd(const char *cmd, FILE *out) {
    FILE *p = popen(cmd, "r");
    if (!p) {
        fprintf(out, "(Error al ejecutar: %s) errno=%d\n", cmd, errno);
        return -1;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), p)) fputs(buf, out);
    pclose(p);
    return 0;
}

// Genera sysinfo.txt con la info que se nos pide
static void saveSysInfo(const char *outfile) {
    FILE *f = fopen(outfile, "w");
    if (!f) {
        perror("[-] Error al abrir sysinfo.txt");
        return;
    }

    write_line(f, "OS y Kernel");
    dump_cmd("uname -a", f);

    write_line(f, "Distribucion");
    // lsb_release puede no estar instalado; caemos a /etc/os-release
    dump_cmd("lsb_release -a 2>/dev/null || cat /etc/os-release", f);

    write_line(f, "IPs");
    dump_cmd("ip -4 -o addr show | awk '{print $2, $4}'", f);

    write_line(f, "CPU Info / Nucleos");
    dump_cmd("lscpu", f);
    dump_cmd("echo -n 'nproc: '; nproc", f);

    write_line(f, "Arquitectura");
    dump_cmd("uname -m", f);

    write_line(f, "Memoria");
    dump_cmd("free -h", f);

    write_line(f, "Discos");
    dump_cmd("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT", f);

    write_line(f, "Usuarios conectados");
    dump_cmd("who", f);

    write_line(f, "Todos los usuarios del sistema");
    dump_cmd("getent passwd | cut -d: -f1", f);

    write_line(f, "Uptime");
    dump_cmd("uptime -p; uptime -s", f);

    write_line(f, "Procesos activos (conteo)");
    dump_cmd("ps -e --no-headers | wc -l", f);

    write_line(f, "Directorios montados");
    dump_cmd("mount | column -t", f);

    fclose(f);
}

int main(void) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error al crear el socket");
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        return 1;
    }
    if (listen(server_sock, 1) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Server listening port %d...\n", PORT);

    socklen_t addr_size = sizeof(client_addr);
    int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error en accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client connected from %s\n", inet_ntoa(client_addr.sin_addr));

    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift = 0;

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';
    sscanf(buffer, "%1023s %d", clave, &shift);
    printf("[+][Server] Encrypted key obtained: %s (shift=%d)\n", clave, shift);

    if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);

        const char *ok = "ACCESS GRANTED";
        send(client_sock, ok, (int)strlen(ok), 0);

        // generar y enviar sysinfo.txt
        saveSysInfo("sysinfo.txt");
        // pequeña pausa para evitar mezclar texto del banner con binario del archivo
        usleep(200 * 1000); // 200 ms
        sendFile("sysinfo.txt", client_sock);
        printf("[+] Sent file: sysinfo.txt\n");
    } else {
        const char *bad = "ACCESS DENIED";
        send(client_sock, bad, (int)strlen(bad), 0);
        printf("[-][Server] Wrong Key\n");
    }

    close(client_sock);
    close(server_sock);
    return 0;
}
