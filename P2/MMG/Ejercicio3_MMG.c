// server.c
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

void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL) {
        perror("Error!");
        return;
    }

    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error to open the file");
        pclose(fpCommand);
        return;
    }

    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
    }

    fclose(fpOutput);
    pclose(fpCommand);
}

void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Cannot open the file");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

// to lower
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// trim
void trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

static void write_line(FILE *out, const char *title) {
    fprintf(out, "\n=== %s ===\n", title);
}

// Se ejecuta el comando y se guarda la salida en el archivo
static void write_cmd(FILE *out, const char *title, const char *cmd) {
    write_line(out, title);
    FILE *fp = popen(cmd, "r");  // se abre la salida del comando para leerla
    if (!fp) {
        fprintf(out, "[ERROR] '%s': %s\n", cmd, strerror(errno));
        return;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp))  // lee y escribe línea por línea
        fputs(buf, out);
    pclose(fp);
}

// Generamos sysinfo.txt con la información del sistema
static void writeSysInfo(const char *outfile) {
    FILE *out = fopen(outfile, "w");
    if (!out) return;

    write_cmd(out, "OS y Kernel", "uname -a");
    write_cmd(out, "Distribucion", "grep -E '^(NAME|VERSION)=' /etc/os-release | sed 's/\\\"//g'");
    write_cmd(out, "IPs", "ip -br addr show");
    write_cmd(out, "CPU", "grep -m1 'model name' /proc/cpuinfo");
    write_cmd(out, "Nucleos", "nproc");
    write_cmd(out, "Arquitectura", "uname -m");
    write_cmd(out, "Memoria", "free -h");
    write_cmd(out, "Discos", "df -hT");
    write_cmd(out, "Usuarios conectados", "who");
    write_cmd(out, "Todos los usuarios", "cut -d: -f1 /etc/passwd");
    write_cmd(out, "Uptime", "uptime -p");
    write_cmd(out, "Procesos activos", "ps -e --no-headers | wc -l");
    write_cmd(out, "Directorios montados", "mount | sed 's/ (.*)//'");

    fclose(out);
}

bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;

    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0';
    trim(buffer);
    toLowerCase(buffer);

    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL) {
        printf("[-]Error opening file!\n");
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buffer) == 0) {
            foundWorld = true;
            break;
        }
    }
    fclose(fp);
    return foundWorld;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Server listening port %d...\n", PORT);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client conneted\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }

    buffer[bytes] = '\0';
    sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave)) {
	decryptCaesar(clave, shift);
	printf("[+][Server] Key decrypted: %s\n", clave);


	send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
	sleep(1); // hacemos una pausa para no mezclar mensajes con el archivo

	// Generamos y enviamos el sysinfo.txt que pide la práctica
	writeSysInfo("sysinfo.txt");
	sendFile("sysinfo.txt", client_sock);    // se lo mandamos al cliente
	printf("[+] Sent file 'sysinfo.txt'\n");

    } else {
    	send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
    	printf("[-][Server] Wrong Key\n");
    }


    close(client_sock);
    close(server_sock);
    return 0;
}
