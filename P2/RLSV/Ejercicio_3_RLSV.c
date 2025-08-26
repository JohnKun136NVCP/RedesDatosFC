#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>       // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>      // Funciones generales (exit, malloc, etc.)
#include <string.h>      // Manejo de cadenas (strlen, strcpy, etc.)
#include <ctype.h>
#include <stdbool.h>     // Tipo booleano (true/false)

#define PORT 7006        // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

/* Función de cifrado César a la inversa */
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

/* Se usa para enviar información de red al cliente
   si la clave es válida. */
void saveNetworkInfo(const char *outputFile) {
        FILE *fpCommand;
        FILE *fpOutput;
        char buffer[512];
        // Ejecutar comando para obtener información de red
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
        // Leer línea por línea y escribir en el archivo
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
                fputs(buffer, fpOutput);
        }
        // Cerrar ambos archivos
        fclose(fpOutput);
        pclose(fpCommand);
}

/* Abre el archivo en modo lectura y lo envía en bloques al cliente usando send.*/
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

/* Compara la clave recibida con las líneas del archivo cipherworlds.txt
Si encuentra coincidencia exacta (ignorando mayúsculas y espacios),
devuelve true.*/

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


// Función main
int main() {
        int server_sock, client_sock;
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_size;
        char buffer[BUFFER_SIZE] = {0};
        char clave[BUFFER_SIZE];
        int shift;
        // Crea un socket.
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == -1) {
                perror("[-] Error to create the socket");
                return 1;
        }
        // Configura la dirección del servidor (IPv4, puerto, cualquier IP local).
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        // Asocia el socket al puerto.
        if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))
        < 0) {
                perror("[-] Error binding");
                close(server_sock);
                return 1;
        }
        // Pone el socket en modo escucha.
        if (listen(server_sock, 1) < 0) {
                perror("[-] Error on listen");
                close(server_sock);
                return 1;
        }
        // Espera y acepta una conexión entrante.
        printf("[+] Server listening port %d...\n", PORT);
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
        if (client_sock < 0) {
                perror("[-] Error on accept");
                close(server_sock);
                return 1;
        }
        // Recibe la clave cifrada y el desplazamiento.
        printf("[+] Client conneted\n");
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
                printf("[-] Missed key\n");
                close(client_sock);
                close(server_sock);
                return 1;
        }
        // Extrae ambos usando sscanf.
        buffer[bytes] = '\0';
        sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
        printf("[+][Server] Encrypted key obtained: %s\n", clave);
        // Si la clave está en el archivo, la descifra,
        // envía confirmación y luego el archivo de red.
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
        // Libera los recursos de red.
        close(client_sock);
        close(server_sock);
        return 0;
}