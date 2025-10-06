#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define BUFFER_SIZE 2048
#define PORT 49200 //definimos un sólo puerto para todos los alias 

// Función para obtener día, fecha y hora 
void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", t);
}

//verifica que se pasen los dos argumentos, en este caso el servidor y el archivo
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <servidor> <archivo>\n", argv[0]);
        return 1;
    }

    char *server_alias = argv[1]; //alias
    char *filename = argv[2]; //nombre de archivo
    char timestamp[20]; // almacenar la hora

    // Lee el archivo
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("[-] fopen");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *file_content = (char *)malloc(file_size + 1);
    if (!file_content) {
        perror("[-] malloc");
        fclose(fp);
        return 1;
    }

    fread(file_content, 1, file_size, fp);
    file_content[file_size] = '\0';
    fclose(fp);

    // Crea el socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] socket");
        free(file_content);
        return 1;
    }

    // crea un socket TCP
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Usar el alias (debe estar en /etc/hosts)
    if (inet_pton(AF_INET, server_alias, &serv_addr.sin_addr) <= 0) {
        printf("[-] Error: No se pudo resolver el alias %s\n", server_alias);
        close(sock);
        free(file_content);
        return 1;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[-] No se pudo conectar a %s:%d\n", server_alias, PORT);
        close(sock);
        free(file_content);
        return 1;
    }

    // Enviar archivo
    if (send(sock, file_content, strlen(file_content), 0) < 0) {
        perror("[-] send");
        close(sock);
        free(file_content);
        return 1;
    }

    printf("[Cliente] Archivo %s enviado a %s\n", filename, server_alias);

    // Recibe respuesta y obtiene la hora actual
    char buffer[BUFFER_SIZE];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        get_timestamp(timestamp);
        
        // Guardar en archivo con los datos que obtuvimos 
        FILE *log = fopen("status.log", "a");
        if (log) {
            fprintf(log, "%s %s %s\n", timestamp, server_alias, buffer);
            fclose(log);
        }
        
        printf("[Cliente] Respuesta: %s\n", buffer);
    }

    close(sock);
    free(file_content);
    return 0;
}