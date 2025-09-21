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
#define BUFFER_SIZE 1024 //Tamaño del buffer

void decryptCaesar(char *text, int shift) {
        shift = shift % 26;
        //Para cada caracter en la cadena dada, si se trata de una letra ya sea mayuscula
        //o minuscula lo mueve hacia atrás según el cifrado cesar. Si no corresponde a una letra, lo deja tal y como esta.
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

void infoSistema(){
        FILE* archivo;
        FILE* fp;
        //Creamos o abrimos el archivo
        archivo = fopen("sysinfo.txt", "w+");

        char path[1024];
        //Popen sirve para leer la informacion de un comando como si fuera la de un archivo.
        //uname -a Muestra el nombre del SO y toda la información disponible desde el kernel hasta la version.
        fp = popen("uname -a", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "OS y Kernel \n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //ifconfig Muestra información sobre todas las interfaces de red activas e inactivas:
        fp = popen("ifconfig -a", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "IP\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //lscpu Muestra informacion sobre el CPU
        fp = popen("lscpu", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "CPU\n");
	while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //free Permite ver el estado actual de la memoria
        fp = popen("free", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Memoria\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //df -h muestra información del sistema de archivos y cuanto almacenamiento del disco hay libre
        fp = popen("df -h", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Disco\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //who Muestra información de todos los usuarios actuales en el sistema local
        fp = popen("who", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Usuarios\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        ///etc/passwd/ Archivo que contiene una entrada para cada usuario con sus atributos básicos
        fp = fopen("/etc/passwd", "r");
	if (fp == NULL) {
            printf("Lectura Fallida\n" );
        }

        fprintf(archivo, "%s", "Todos los usuarios\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //uptime Muestra información como la hora actual y cuanto tiempo lleva en marcha el sistema.
        fp = popen("uptime", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Tiempo\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //ps aux Se usa para obtener información sobre los procesos actuales
        fp = popen("ps aux", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Procesos Activos\n");

        while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }
        //mount Muestra una lista de sistemas de archivos  montados en el sistema
        fp = popen("mount", "r");
        if (fp == NULL) {
            printf("Comando Fallido\n" );
        }

        fprintf(archivo, "%s", "Directorios Montados\n");
	while (fgets(path, sizeof(path), fp) != NULL) {
            fprintf(archivo, "%s", path);
        }

        pclose(fp);

        fclose(archivo);
}

// Guarda la salida en un archivo especificado. Se usa para enviar informacion de red al cliente si la clave es valida.
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
        //Abre el archivo en solo lectura (r)
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

// Funcion para convertir a minusculas
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

//Verifica la llave dada por el usuario, buscando alguna coincidencia en el archivo cipherworlds.txt 
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
                printf("%s", line);
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
        //Crea un socket.
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == -1) {
                perror("[-] Error to create the socket");
                return 1;
        }
        //Configura la direccion del servidor (IPv4, puerto, cualquier IP local).
        server_addr.sin_family = AF_INET;
        //Asocia el socket al puerto.
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        //Pone el socket en modo escucha.
        if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))< 0) {
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
        //Espera y acepta una conexi ́on entrante.
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
        if (client_sock < 0) {
                perror("[-] Error on accept");
                close(server_sock);
                return 1;
        }
	printf("[+] Client conneted\n");
        //Recibe la clave cifrada y el desplazamiento.
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
                printf("[-] Missed key\n");
                close(client_sock);
                close(server_sock);
                return 1;
        }
        buffer[bytes] = '\0';
        //Extrae clave y desplazamiento usando sscanf
        sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
        printf("[+][Server] Encrypted key obtained: %s\n", clave);
        //Si la clave esta en el archivo, la descifra, env ́ıa confirmacion y luego el archivo de red.
        if (isOnFile(clave)) {
                decryptCaesar(clave, shift);
                printf("[+][Server] Key decrypted: %s\n", clave);
                send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
                sleep(1); // Pequenia pausa para evitar colision de datos
                //Creacion y Envio de Archivos
                saveNetworkInfo("network_info.txt");
                infoSistema();
                sendFile("network_info.txt", client_sock);
                sleep(2);
                sendFile("sysinfo.txt", client_sock);
                printf("[+] Sent file\n");
        } else {
                send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
                printf("[-][Server] Wrong Key\n");
        }
        //Libera los recursos de red.
        close(client_sock);
        close(server_sock);
        return 0;
}
