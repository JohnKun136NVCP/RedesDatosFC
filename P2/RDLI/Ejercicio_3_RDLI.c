#include <time.h>
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

// Guarda información del sistema en sysinfo.txt
void saveSysInfo(const char *outputFile) {
        FILE *fp = fopen(outputFile, "w");
        if (!fp) {
                perror("[-] Error al abrir sysinfo.txt");
                return;
        }

        char buffer[512];
        FILE *cmd;

        // Ocupamos el parámetro r para leer cada salida
        fprintf(fp, "OS y Kernel: " "\n");
        cmd = popen("uname -o -r -s", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Distribución:\n");
        cmd = popen("cat /etc/*release", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "IPs:\n");
        cmd = popen("hostname -I", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "CPU Info:\n");
        cmd = popen("lscpu | grep 'Model name'", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Núcleos:\n");
        cmd = popen("nproc", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Arquitectura:\n");
        cmd = popen("uname -m", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Memoria:\n");
        cmd = popen("free -h", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Disco:\n");
        cmd = popen("df -h", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Usuarios conectados:\n");
        cmd = popen("who", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Todos los usuarios del sistema:\n");
        cmd = popen("cut -d: -f1 /etc/passwd", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Uptime:\n");
        cmd = popen("uptime -p", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Procesos activos:\n");
        cmd = popen("ps -e --no-headers | wc -l", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        fprintf(fp, "Directorios montados:\n");
        cmd = popen("mount | grep '^/'", "r");
        if (cmd) { while (fgets(buffer, sizeof(buffer), cmd)) fputs(buffer, fp); pclose(cmd); }
        // Cerramos el archivo
        fclose(fp);
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


void saveNetworkInfo(const char *outputFile){
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
        // Leer linea por linea y escribir en el archivo
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

// Función para convertir a minúsculas
void toLowerCase(char *str) {
for (int i = 0; str[i]; i++)
str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
void trim(char *str) {
        char *end;
        while (isspace((unsigned char)*str)) str++; // inicio
        end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end)) end--; // final
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
        }else{
                printf("[+] File opened successfully\n");
        }
        while (fgets(line, sizeof(line), fp) != NULL) {
                line[strcspn(line, "\n")] = '\0';
                trim(line);
                toLowerCase(line);
                if (strcmp(line, buffer) == 0) {
                        foundWorld = true;
                        break;
                }else{
                        printf("[-] Word not found: %s\n", line);
                }
        }
        fclose(fp);
        return foundWorld;
}

int main() {

        int server_sock, client_sock;

        // Guardar información del sistema al iniciar
        saveSysInfo("sysinfo.txt");
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_size;
        char buffer[BUFFER_SIZE] = {0};
        char clave[BUFFER_SIZE];
        int shift;

        // Crea un socket
        server_sock = socket(AF_INET, SOCK_STREAM, 0);

        if (server_sock == -1) {
                perror("[-] Error to create the socket");
                return 1;
        }

        // Asocia un puerto al socket
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = INADDR_ANY;


        if (bind(server_sock, (struct sockaddr *) & server_addr,sizeof(server_addr))< 0) {
                perror("[-] Error binding");
                close(server_sock);
                return 1;
        }

        // Pone el socket en modo escucha
        if (listen(server_sock, 1) < 0) {
                perror("[-] Error on listen");
                close(server_sock);
                return 1;
        }

        printf("[+] Server listening port %d...\n", PORT);

        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);

        // Espera y acepta una conexion entrante
        if (client_sock < 0) {
                perror("[-] Error on accept");
                close(server_sock);
                return 1;
        }

        printf("[+] Client connected\n");

        // Recibe la clave sifrada y el desplazamiento
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

        // Si la clave esta en el archivo la desifra, envia confirmacion y luefo el archivo de red
        if (isOnFile(clave)) {
                decryptCaesar(clave, shift);
                printf("[+][Server] Key decrypted: %s\n", clave);
                send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
                sleep(1); // Peque~na pausa para evitar colisi ́on de datos
                saveNetworkInfo("network_info.txt");
                sendFile("network_info.txt", client_sock);
                printf("[+] Sent file\n");
        } else {
                send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
                printf("[-][Server] Wrong Key\n");
        }

        // Luego libera los recursos de red
        close(client_sock);
        close(server_sock);
        return 0;
}