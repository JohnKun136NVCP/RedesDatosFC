#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

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

//Funcion que cifra
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            //Para cifrar el texto en vez de restar el desplazamiento, hay que sumarselo
            //Eliminamos el +26 porque el desplazamiento que nos den siempre será positivo
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            //Mismo caso que el de arriba pero con minusculas
            text[i] = ((c - 'a' + shift) % 26) + 'a';
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

//Funcion que guarda la informacion del sistema
void saveSystemInfo() {
    FILE *fp;
    char buffer[512];
    
    //Abrir el archivo para escribir
    fp = fopen("sysinfo.txt", "w");
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return;
    }
    
    //Codigo que escribe el OS y Kernel
    fprintf(fp, "OS y Kernel: ");
    FILE *cmd = popen("uname -s -r", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe la distribución
    fprintf(fp, "Distribución: ");
    cmd = popen("hostnamectl | grep 'Operating System'", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe las IPs
    fprintf(fp, "IPs: ");
    cmd = popen("ip addr show | grep 'inet '", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe la info del CPU
    fprintf(fp, "Informacion del CPU: ");
    cmd = popen("lscpu | grep 'Model name'", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe los núcleos
    fprintf(fp, "Nucleos: ");
    cmd = popen("nproc", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe la arquitectura
    fprintf(fp, "Arquitectura: ");
    cmd = popen("uname -m", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe la memoria
    fprintf(fp, "Memoria: ");
    cmd = popen("free -h", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe el disco
    fprintf(fp, "Disco: ");
    cmd = popen("df -h", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe los usuarios conectados
    fprintf(fp, "Usuarios conectados: ");
    cmd = popen("who", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe todos los usuarios del sistema
    fprintf(fp, "Todos los usuarios del sistema: ");
    cmd = popen("cut -d: -f1 /etc/passwd", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe el uptime
    fprintf(fp, "Uptime: ");
    cmd = popen("uptime", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe los procesos activos
    fprintf(fp, "Procesos activos: ");
    cmd = popen("ps aux | wc -l", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "Number of processes: %s", buffer);
        }
        pclose(cmd);
    }
    fprintf(fp, "\n");
    
    //Codigo que escribe los directorios montados
    fprintf(fp, "Directorios montados: ");
    cmd = popen("mount", "r");
    if (cmd) {
        while (fgets(buffer, sizeof(buffer), cmd) != NULL) {
            fprintf(fp, "%s", buffer);
        }
        pclose(cmd);
    }
    
    fclose(fp);
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

// Funcion para convertir a min ́usculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Funcion para eliminar espacios al inicio y final
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
    int ports[] = {49200, 49201, 49202};
    int num_ports = 3;
    int server_socks[3];
    struct sockaddr_in server_addr;

    // Crear y bindear los sockets
    for (int i = 0; i < num_ports; i++) {
        server_socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socks[i] < 0) { perror("socket"); exit(1); }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(ports[i]);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socks[i], (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind"); exit(1);
        }

        if (listen(server_socks[i], 5) < 0) {
            perror("listen"); exit(1);
        }

        printf("[+] Listening on port %d\n", ports[i]);
    }

    fd_set readfds;
    int max_fd = 0;
    while (1) {
        FD_ZERO(&readfds);
        max_fd = 0;
        for (int i = 0; i < num_ports; i++) {
            FD_SET(server_socks[i], &readfds);
            if (server_socks[i] > max_fd) max_fd = server_socks[i];
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select"); exit(1);
        }

        // Verificar qué socket tiene conexión
        for (int i = 0; i < num_ports; i++) {
            if (FD_ISSET(server_socks[i], &readfds)) {
                int client_sock;
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                client_sock = accept(server_socks[i], (struct sockaddr*)&client_addr, &addr_size);
                if (client_sock < 0) { perror("accept"); continue; }

                // Recibir datos del cliente
                char buffer[BUFFER_SIZE];
                int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) { close(client_sock); continue; }
                buffer[bytes] = '\0';

                char clave[BUFFER_SIZE];
                int shift;
                sscanf(buffer, "%s %d", clave, &shift);

                printf("[+] Received key: %s, shift: %d on port %d\n", clave, shift, ports[i]);

                // Verificar clave
                if (isOnFile(clave)) {
                    decryptCaesar(clave, shift);
                    send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
                    sleep(1);
                    saveNetworkInfo("sysinfo.txt");
                    sendFile("sysinfo.txt", client_sock);
                    printf("[-]Sent system info file\n")
                    saveNetworkInfo("network_info.txt");
                    sendFile("network_info", client_sock);
                    printf("[-]Sent network info file\n")
                } else {
                    send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
                }

                close(client_sock);
            }
        }
    }

    // Cerrar sockets
    for (int i = 0; i < num_ports; i++) close(server_socks[i]);
    return 0;
}