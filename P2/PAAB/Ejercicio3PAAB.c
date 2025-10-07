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


void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}


void trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; 
    *(end + 1) = '\0';
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
// Funcion auxiliar para sysinfo
static int write_section(FILE *fo, const char *titulo, const char *cmd) {
    fprintf(fo, "==== %s ====\n", titulo);
    FILE *p = popen(cmd, "r");
    if (!p) { fprintf(fo, "[error al ejecutar: %s]\n\n", cmd); return -1; }
    char buf[2048]; size_t wrote = 0;
    while (fgets(buf, sizeof buf, p)) { fputs(buf, fo); wrote += strlen(buf); }
    pclose(p);
    if (wrote == 0) fputs("[sin salida]\n", fo);
    fputc('\n', fo);
    return 0;
}
//Genera sysinfo
int generate_sysinfo(const char *path) {
    FILE *fo = fopen(path, "w");
    if (!fo) { perror("sysinfo fopen"); return -1; }

    // cabecera con fecha y host
    write_section(fo, "Fecha y hora", "date");
    write_section(fo, "Hostname", "hostname");

    // OS y Kernel
    write_section(fo, "OS y Kernel", "uname -sr");

    // Distribucion: intenta lsb_release, si no, /etc/os-release
    if (write_section(fo, "Distribucion", "lsb_release -d 2>/dev/null") < 0) {
        write_section(fo, "Distribucion (/etc/os-release)",
                      "grep PRETTY_NAME /etc/os-release | cut -d= -f2 | tr -d '\"'");
    }

    // IPs
    write_section(fo, "IPs (IPv4)", "ip -4 -o addr show | awk '{print $2, $4}'");

    // CPU / Núcleos / Arquitectura
    write_section(fo, "CPU Info", "lscpu");
    write_section(fo, "Nucleos", "nproc");
    write_section(fo, "Arquitectura", "uname -m");

    // Memoria y Disco
    write_section(fo, "Memoria", "free -h");
    write_section(fo, "Disco", "df -h");

    // Usuarios
    write_section(fo, "Usuarios conectados", "who");
    write_section(fo, "Todos los usuarios del sistema", "getent passwd | cut -d: -f1");

    // Uptime y procesos
    write_section(fo, "Uptime", "uptime -p");
    write_section(fo, "Procesos activos (conteo)", "ps -e --no-headers | wc -l");

    // Directorios montados
    write_section(fo, "Directorios montados", "mount");

    fclose(fo);
    return 0;
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

    if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))
        < 0) {
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
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
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
        sleep(1); // pausa para evitar colision de datos
        saveNetworkInfo("network_info.txt");
        sendFile("network_info.txt", client_sock);
        printf("[+] Sent file\n");
        //Generar sysinfo
        if (generate_sysinfo("sysinfo.txt") == 0)
            printf("[+] sysinfo.txt generado\n");
        else
            printf("[-] No se pudo generar sysinfo.txt\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }
    close(client_sock);
    close(server_sock);
    return 0;
}