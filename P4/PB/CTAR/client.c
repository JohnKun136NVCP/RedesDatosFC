#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 1024
char *servers[] = {"s01", "s02", "s03", "s04"};

//clientB.c

/*
    Función para guardar fecha, hora, estado, nombre de archivo y servidor
*/
void saveLog(const char *status, const char *filename, const char *server) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    FILE *log_file = fopen("clientLog.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s | %s | %s | %s\n", timestamp, status, filename, server);
        fclose(log_file);
    }
}

/*
    Función principal para conectar al servidor, recibir un puerto dinámico, conectarse a él y enviar o recibir datos
    a los servidores restantes
*/
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("USE: %s <SERVER> <PORT> <FILE>\n", argv[0]);
        printf("Example: %s s01 49200 file1.txt\n", argv[0]);
        exit(1);
    }

    int client_sock;
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];
    struct sockaddr_in serv_addr;
    int dynamic_port;
    struct addrinfo name, *res;
    // Leer archivo
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening file");
        saveLog("ERROR", filename, "File not found");
        exit(1);
    }

    char file_content[BUFFER_SIZE] = {0};
    size_t bytes_read = fread(file_content, 1, sizeof(file_content) - 1, fp);
    file_content[bytes_read] = '\0';
    fclose(fp);

    for (int i = 0; i < 4; i++) {
        if (strcmp(server_ip, servers[i]) == 0){
            continue;
        }
        //Obtenemos la dirección ip atraves del alias
        memset(&name, 0, sizeof name);
        name.ai_family = AF_INET;
        name.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(servers[i], NULL, &name, &res) != 0) {
            perror("Error resolving hostname");
            saveLog("ERROR", filename, servers[i]);
            exit(1);
        }

        //Creamos el socket del cliente para la comunicación
        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock < 0) {
            perror("Socket creation failed");
            freeaddrinfo(res);
            exit(1);
        }
        
        // Configuramos la dirección del cliente
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        serv_addr.sin_addr = ipv4->sin_addr;
        
        freeaddrinfo(res);  

        //Nos conectamos al servidor
        if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection failed");
            close(client_sock);
            exit(1);
        }

        // Recibimos el puerto dinámico
        char port_response[64] = {0};
        int bytes_received = recv(client_sock, port_response, sizeof(port_response) - 1, 0);
        if (bytes_received <= 0) {
            perror("Error receiving port");
            close(client_sock);
            exit(1);
        }
        port_response[bytes_received] = '\0';
        close(client_sock);

        // Nos conectamos al puerto dinámico recibido
        if (sscanf(port_response, "DYNAMIC_PORT|%d", &dynamic_port) == 1) {
            int dynamic_sock = socket(AF_INET, SOCK_STREAM, 0);
            serv_addr.sin_port = htons(dynamic_port);
            
            if (connect(dynamic_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection to dynamic port failed");
                exit(1);
            }

            char buffer[BUFFER_SIZE*2];
            snprintf(buffer, sizeof(buffer), "%s|%s|%s", servers[i], filename, file_content);

            if (send(dynamic_sock, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                close(dynamic_sock);
                exit(1);
            }

            char response[BUFFER_SIZE] = {0};
            int bytes = recv(dynamic_sock, response, sizeof(response) - 1, 0);
            if (bytes > 0) {
                response[bytes] = '\0';
                printf("SERVER RESPONSE from %s: %s\n", servers[i], response);
                saveLog("SUCCESS", filename, servers[i]);
            } else {
                printf("No response from server\n");
            }
            
            close(dynamic_sock);
        }
    }
    return 0;
}
