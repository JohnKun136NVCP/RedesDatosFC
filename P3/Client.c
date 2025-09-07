#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define PORT 49200
#define BUFFER_SIZE 1024

// Funcion para eliminar espacios al inicio y final
void trim(char *str) {
        char *end;
        while (isspace((unsigned char)*str)) str++; // inicio
                end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end)) end--; // final
        *(end + 1) = '\0';
}

//Guardamos el archivo en un buffer y lo limpiamos para  mandarlo
void sendFile(const char *filename, char buffer[]) {
        //Abre el archivo en solo lectura (r)
        size_t bytesRead;
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
                perror("[-] Cannot open the file");
                return;
        }

        while ((bytesRead = fread(buffer, 1, BUFFER_SIZE - 1, fp)) > 0){
                buffer[bytesRead] = '\0';
                //printf("%s", buffer);
        }
        trim(buffer);
        fclose(fp);
}

int main(int argc, char *argv[]) {
        if (argc != 4) {
                printf("Type: %s <puerto> <desplazamiento> <archivo>\n", argv[0]);
                return 1;
        }

        char *puerto = argv[1];
        int objetivo = atoi(puerto);
        if(objetivo == 0){
                printf("Puerto Invalido");
                return 1;
        }
        char *shift = argv[2];
        int desplazamiento = atoi(shift);
        if(desplazamiento == 0){
                printf("Desplazamiento Invalido");
                return 1;
        }
        char *archivo = argv[3];        
        int todos_puertos[3];
        todos_puertos[0] = 49200;
        todos_puertos[1] = 49201;
        todos_puertos[2] = 49202;

        int client_sock[3]; 
        struct sockaddr_in serv_addr;
        char *server_ip = "192.168.100.56";
        for (int i = 0; i < (sizeof(client_sock)/sizeof(client_sock[0])); i++){
                client_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (client_sock[i] == -1) {
                        perror("[-] Error to create the socket");
                        return 1;
                }
        }
        serv_addr.sin_family = AF_INET; 

        for (int i = 0; i < (sizeof(todos_puertos)/sizeof(todos_puertos[0])); i++){ 
                char buffer[BUFFER_SIZE] = {0};
                char mensaje[BUFFER_SIZE];
                serv_addr.sin_port = htons(todos_puertos[i]);
                serv_addr.sin_addr.s_addr = inet_addr(server_ip);
                int respuesta = connect(client_sock[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                if (respuesta < 0) {
                        printf("%d", respuesta);
                        perror("[-] Error to connect");
                        close(client_sock[i]);
                        break;
                }

                char clave[BUFFER_SIZE] = {0};
                sendFile(archivo, clave); 
                snprintf(mensaje, sizeof(mensaje), "puerto: %d llave: %d contenido: %s", objetivo, desplazamiento, clave);
                send(client_sock[i], mensaje, strlen(mensaje), 0);

                int bytes = recv(client_sock[i], buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                        buffer[bytes] = '\0';
                        printf("[*]SERVER  RESPONSE %d: %s\n",todos_puertos[i], buffer);
                        if(strstr(buffer, "REJECTED") == NULL){
                                i = 4;
                        }
                } else {
                        printf("[-] Server connection tiemeout\n");
                }
                close(client_sock[i]);
        }
        return 0;
}
