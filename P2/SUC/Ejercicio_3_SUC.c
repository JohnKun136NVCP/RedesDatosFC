#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

// Tamaño de buffer para recolectar salida de comandos del sistema
#define SYSINFO_CMD_BUF 1024

// Escribe el contenido de un comando de sistema en un archivo ya abierto,
// precedido por un encabezado legible.
static void appendCommandOutput(FILE *fpOutput, const char *sectionTitle, const char *command) {
    if (fpOutput == NULL || sectionTitle == NULL || command == NULL) {
        return;
    }
    fprintf(fpOutput, "==== %s ====\n", sectionTitle);

    FILE *fpCommand = popen(command, "r");
    if (fpCommand == NULL) {
        fprintf(fpOutput, "(no disponible)\n\n");
        return;
    }

    char buffer[SYSINFO_CMD_BUF];
    bool wroteSomething = false;
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
        wroteSomething = true;
    }
    if (!wroteSomething) {
        fprintf(fpOutput, "(sin datos)\n");
    }
    fputc('\n', fpOutput);
    pclose(fpCommand);
}

// Obtiene información variada del sistema y la guarda en un archivo de texto.
// Archivo destino: sysinfo.txt (o el indicado por parámetro)
static void saveSystemInfo(const char *outputFile) {
    const char *filename = (outputFile && *outputFile) ? outputFile : "sysinfo.txt";
    FILE *fpOutput = fopen(filename, "w");
    if (fpOutput == NULL) {
        perror("[-] Error to open sysinfo file");
        return;
    }

    // OS y Kernel
    appendCommandOutput(fpOutput, "OS y Kernel", "uname -srm 2>/dev/null");

    // Distribución (intenta lsb_release, luego /etc/os-release)
    appendCommandOutput(fpOutput, "Distribución", "lsb_release -ds 2>/dev/null || (grep ^PRETTY_NAME= /etc/os-release 2>/dev/null | cut -d= -f2- | tr -d '\"') || head -n1 /etc/*-release 2>/dev/null");

    // IPs
    appendCommandOutput(fpOutput, "IPs", "hostname -I 2>/dev/null || ip -4 -o addr show 2>/dev/null | awk '{print $4}'");

    // CPU Info (modelo), Núcleos y Arquitectura
    appendCommandOutput(fpOutput, "CPU Info", "lscpu 2>/dev/null | egrep \"Model name|Vendor ID\" || grep -m1 \"model name\" /proc/cpuinfo 2>/dev/null");
    appendCommandOutput(fpOutput, "Núcleos", "lscpu 2>/dev/null | grep '^CPU(s):' | awk '{print $2}' || nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null");
    appendCommandOutput(fpOutput, "Arquitectura", "uname -m 2>/dev/null");

    // Memoria
    appendCommandOutput(fpOutput, "Memoria", "free -h 2>/dev/null || vm_stat 2>/dev/null");

    // Disco
    appendCommandOutput(fpOutput, "Disco", "df -h 2>/dev/null");

    // Usuarios conectados
    appendCommandOutput(fpOutput, "Usuarios conectados", "who 2>/dev/null || users 2>/dev/null");

    // Todos los usuarios del sistema
    appendCommandOutput(fpOutput, "Todos los usuarios del sistema", "cut -d: -f1 /etc/passwd 2>/dev/null");

    // Uptime
    appendCommandOutput(fpOutput, "Uptime", "uptime -p 2>/dev/null || uptime 2>/dev/null");

    // Procesos activos (top por CPU, top 20)
    appendCommandOutput(fpOutput, "Procesos activos (top 20 por CPU)", "ps -eo pid,comm,stat,pcpu,pmem --sort=-pcpu 2>/dev/null | head -n 21");

    // Directorios montados
    appendCommandOutput(fpOutput, "Directorios montados", "findmnt -rno TARGET,SOURCE,FSTYPE,OPTIONS 2>/dev/null || mount 2>/dev/null");

    fclose(fpOutput);
}

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
    // Ejecutar comando para obtener informaci ́on de red
    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL) {
        perror("Error!");
        return;
    }
    // Abrir archivo para guardar la salida
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error to open the file");
        pclose(fpCommand);
        return;
    }
    // Leer l ́ınea por l ́ınea y escribir en el archivo
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
    }
    // Cerrar ambos archivos
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

// Descifra cada línea del archivo de entrada usando el desplazamiento dado
// y escribe el resultado en el archivo de salida.
static void decryptCipherwordsFile(const char *inputFile, const char *outputFile, int shift) {
    const char *inName = (inputFile && *inputFile) ? inputFile : "cipherworlds.txt";
    const char *outName = (outputFile && *outputFile) ? outputFile : "cipherworlds_decrypted.txt";

    FILE *in = fopen(inName, "r");
    if (in == NULL) {
        perror("[-] Error opening input cipherworlds file");
        return;
    }
    FILE *out = fopen(outName, "w");
    if (out == NULL) {
        perror("[-] Error opening output decrypted file");
        fclose(in);
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), in) != NULL) {
        // Eliminar salto de línea
        line[strcspn(line, "\r\n")] = '\0';
        // Descifrar en sitio
        decryptCaesar(line, shift);
        fputs(line, out);
        fputc('\n', out);
    }

    fclose(out);
    fclose(in);
}

// Funci ́on para convertir a min ́usculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}
// Funci ́on para eliminar espacios al inicio y final
void trim(char *str) {
    if (str == NULL) return;
    // Trim inicio
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    // Si quedó vacío tras el trim inicial
    if (*str == '\0') return;
    // Trim final
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;
    // Copiamos el buffer original para poder modificarlo
    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminaci ́on
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
        sleep(1); // Peque~na pausa para evitar colisi ́on de datos
        // Genera sysinfo.txt con información del sistema
        saveSystemInfo("sysinfo.txt");
        // También se conserva la funcionalidad de red, si se desea
        saveNetworkInfo("network_info.txt");
        // Descifra todo el archivo cipherworlds.txt con el mismo desplazamiento
        decryptCipherwordsFile("cipherworlds.txt", "cipherworlds_decrypted.txt", shift);
        // Enviar el archivo sysinfo primero
        sendFile("sysinfo.txt", client_sock);
        // Luego, opcionalmente, el archivo de red
        sendFile("network_info.txt", client_sock);
        // Finalmente, enviar el archivo con las palabras descifradas
        sendFile("cipherworlds_decrypted.txt", client_sock);
        printf("[+] Sent file\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }
    close(client_sock);
    close(server_sock);
    return 0;
}