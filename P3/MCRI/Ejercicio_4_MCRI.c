#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT0 49200      // Primer puerto en el que el servidor escucha
#define PORT1 49201      // Segundo puerto en el que el servidor escucha
#define PORT2 49202      // Tercer puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

void decryptCaesar(char *text, int shift){
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++){
        char c = text[i];
        if (isupper(c)){
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        }
        else if (islower(c)){
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        }
        else{
            text[i] = c;
        }
    }
}

void encryptCaesar(char *text, int shift){
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++){
        char c = text[i];
        if (isupper(c)){
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        }
        else if (islower(c)){
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        }
        else{
            text[i] = c;
        }
    }
}

void saveNetworkInfo(const char *outputFile){
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    // Ejecutar comando para obtener información de red
    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL){
        perror("Error!");
        return;
    }

    // Abrir archivo para guardar la salida
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL){
        perror("[-] Error to open the file");
        pclose(fpCommand);
        return;
    }

    // Leer línea por línea y escribir en el archivo
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL){
        fputs(buffer, fpOutput);
    }

    // Cerrar ambos archivos
    fclose(fpOutput);
    pclose(fpCommand);
}

void sendFile(const char *filename, int sockfd)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL){
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0){
        if (send(sockfd, buffer, bytes, 0) == -1){
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

// Función para convertir a minúsculas
void toLowerCase(char *str){
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
void trim(char *str){
    char *end;
    while (isspace((unsigned char)*str))
        str++; // inicio
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--; // final
    *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal){
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;

    // Copiamos el buffer original para poder modificarlo
    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminación

    trim(buffer);
    toLowerCase(buffer);

    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL){
        printf("[-]Error opening file!\n");
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL){
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buffer) == 0){
            foundWorld = true;
            break;
        }
    }

    fclose(fp);
    return foundWorld;
}

void runCommandToFile(FILE *fp, const char *cmd){
    char buffer[256];             // Iniciamos un buffer para leer la salida del comando
    FILE *pipe = popen(cmd, "r"); // Ejecutamos el comando
    if (!pipe){
        fprintf(fp, "Error ejecutando %s\n", cmd); // Si hay error, lo indicamos en el archivo
        return;
    }
    while (fgets(buffer, sizeof(buffer), pipe) != NULL){
        fputs(buffer, fp); // Escribe la salida en el archivo
    }
    pclose(pipe); // Cerramos el pipe
}

void guardar_sysinfo(){
    FILE *fp = fopen("sysinfo.txt", "w"); // Abrimos el archivo para escribir
    if (!fp){
        perror("No se pudo abrir sysinfo.txt"); // Si hay error, lo indicamos
        return;
    }

    fprintf(fp, "OS y Kernel:\n");
    runCommandToFile(fp, "uname -a");

    fprintf(fp, "\nDistribución:\n");
    runCommandToFile(fp, "grep PRETTY_NAME /etc/*release");

    fprintf(fp, "\nIPs:\n");
    runCommandToFile(fp, "hostname -I");

    fprintf(fp, "\nCPU Info:\n");
    runCommandToFile(fp, "lscpu");

    fprintf(fp, "\nNúcleos:\n");
    runCommandToFile(fp, "nproc");

    fprintf(fp, "\nArquitectura:\n");
    runCommandToFile(fp, "uname -m");

    fprintf(fp, "\nMemoria:\n");
    runCommandToFile(fp, "free -h");

    fprintf(fp, "\nDisco:\n");
    runCommandToFile(fp, "df -h");

    fprintf(fp, "\nUsuarios conectados:\n");
    runCommandToFile(fp, "who");

    fprintf(fp, "\nTodos los usuarios del sistema:\n");
    runCommandToFile(fp, "cut -d: -f1 /etc/passwd");

    fprintf(fp, "\nUptime:\n");
    runCommandToFile(fp, "uptime -p");

    fprintf(fp, "\nProcesos activos:\n");
    runCommandToFile(fp, "ps aux | wc -l");

    fprintf(fp, "\nDirectorios montados:\n");
    runCommandToFile(fp, "mount | grep '^/'");

    fclose(fp);
}

int crearSocket(int port){
    int server_sock;
    struct sockaddr_in server_addr;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1){
        perror("[-] Error al crear el socket");
        return 1;
    }

    // Permitir reutilizar la dirección y el puerto
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Enlazar el socket a la dirección y puerto especificados
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[-] Error en bind");
        close(server_sock);
        return -1;
    }

    // Escuchar conexiones entrantes
    if (listen(server_sock, 1) < 0){
        perror("[-] Error en listen");
        close(server_sock);
        return -1;
    }

    printf("[+] Server escuchando puerto %d...\n", port);
    return server_sock;
}

int main()
{
    int puertos[] = {PORT0, PORT1, PORT2};
    int n_puertos = sizeof(puertos) / sizeof(puertos[0]);
    int sockets[n_puertos];
    fd_set readfds;

    guardar_sysinfo();
    saveNetworkInfo("networkinfo.txt");
    
    // Crear sockets para cada puerto
    for (int i = 0; i < n_puertos; i++){
        sockets[i] = crearSocket(puertos[i]);
        if (sockets[i] == -1){
            fprintf(stderr, "[-] Error creando socket en puerto %d\n", puertos[i]);
            for (int j = 0; j < i; j++){
                close(sockets[j]);
            }
            return 1;
        }
    }

    while (1){
        FD_ZERO(&readfds);
        int max_sd = 0;

        // Añadir sockets al conjunto de lectura
        for (int i = 0; i < n_puertos; i++){
            FD_SET(sockets[i], &readfds);
            if (sockets[i] > max_sd){
                max_sd = sockets[i];
            }
        }

        // Esperar a que uno de los sockets tenga actividad
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0){
            perror("[-] Error en select");
            return -1;
        }

        // Comprobar cuál socket tiene actividad
        for (int i = 0; i < n_puertos; i++){
            if (FD_ISSET(sockets[i], &readfds)){
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_sock = accept(sockets[i], (struct sockaddr *)&client_addr, &addr_size);
                ;

                if (client_sock < 0){
                    perror("[-] Error al aceptar");
                    continue;
                }

                printf("[+] Cliente conectado al puerto %d\n", puertos[i]);

                // Recibir puerto solicitado
                int requested_port;
                if (recv(client_sock, &requested_port, sizeof(requested_port), 0) <= 0){
                    printf("[-] No se recibio el puerto solicitado\n");
                    close(client_sock);
                    continue;
                }
                requested_port = ntohl(requested_port);

                int shift;
                if (recv(client_sock, &shift, sizeof(shift), 0) <= 0){
                    printf("[-] No se recibio el desplazamiento\n");
                    close(client_sock);
                    continue;
                }
                shift = ntohl(shift);

                // Validar puerto solicitado
                if (requested_port != puertos[i]){
                    const char *denied = "ACCESO DENEGADO: Puerto incorrecto.\n";
                    send(client_sock, denied, strlen(denied), 0);
                    printf("[-] Solicitud rechazada, el puerto solicitado es %d\n", requested_port);
                    close(client_sock);
                    continue;
                }

                // Recibir nombre del archivo
                char filename[BUFFER_SIZE];
                if (recv(client_sock, &filename, sizeof(filename) - 1, 0) <= 0){
                    printf("[-] No se recibio el nombre del archivo\n");
                    close(client_sock);
                    continue;
                }

                const char *granted = "ACCESO OTORGADO: Enviando archivo.\n";
                send(client_sock, granted, strlen(granted), 0);

                FILE *fp = fopen(filename, "w");
                if (fp == NULL){
                    perror("[-] Error al abrir el archivo");
                    close(client_sock);
                    continue;
                }

                char buffer[BUFFER_SIZE];
                int bytes;
                // Recibir y guardar el archivo
                while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0){
                    encryptCaesar(buffer, shift);
                    fwrite(buffer, 1, bytes, fp);
                }
                fclose(fp);

                // Enviar el archivo cifrado
                fp = fopen(filename, "r");
                if (fp == NULL){
                    perror("[-] Error al abrir el archivo para enviar");
                    close(client_sock);
                    continue;
                }
                while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0){
                    if (send(client_sock, buffer, bytes, 0) == -1){
                        perror("[-] Error al enviar el archivo");
                        return -1;
                    }
                }
                fclose(fp);

                printf("[+] Archivo enviado al cliente y conexión cerrada\n");
                close(client_sock);
            }
        }
    }

    for (int i = 0; i < n_puertos; i++){
        close(sockets[i]);
    }
    return 0;
}