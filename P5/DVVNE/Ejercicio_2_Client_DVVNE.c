#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

//Practica 5
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

int recibir(int client_sock, char buffer[], int bytes, size_t bufft){
        FILE *parchivo;
        bytes = recv(client_sock, buffer, bufft, 0);
        if (bytes <= 0) {
                printf("[-] Missed key\n");
                close(client_sock);
                return 1;
        }
        buffer[bytes] = '\0';
        printf("%s\n", buffer);
        parchivo = fopen("log.txt", "a");
        if (parchivo == NULL) {
                printf("Error al abrir el archivo.\n");
                return 1;
        }
        fputs(buffer, parchivo);
        fclose(parchivo);
        return 0;
}

int main(int argc, char *argv[]) {
        char *newip =  "192.168.0.101";
        if (argc < 3) {
                printf("Type: %s <archivo>\n", argv[0]);
                return 1;
        }
        //Alias e IP's
        char *alias[] = {"s01", "s02", "s03", "s04"};
        char *dIPalias[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103", "192.168.0.104"};

        if(argc > 2){
                if(strcmp(argv[2], "s01") == 0){
                        newip = "192.168.0.101";
                }
                else if(strcmp(argv[2], "s02") == 0){
                        newip = "192.168.0.102";
                }
                else if(strcmp(argv[2], "s03") == 0){
                        newip = "192.168.0.103";
                }
                else if(strcmp(argv[2], "s04") == 0){
                        newip = "192.168.0.104";
                }
        }

        for (int i = 0; i < sizeof(alias)/sizeof(alias[0]); i++){
                if(strcmp(alias[i], argv[2]) != 0){
                        printf("\nServidor: %s\n", dIPalias[i]);
                        char *archivo = argv[1];        
                        int client_sock; 
                        int pnuevo = 0;
                        struct sockaddr_in serv_addr;

                        client_sock = socket(AF_INET, SOCK_STREAM, 0);
                        if (client_sock == -1) {
                                perror("[-] Error to create the socket");
                                return 1;
                        }
                        serv_addr.sin_family = AF_INET;
                        char buffer[BUFFER_SIZE] = {0};
                        char mensaje[BUFFER_SIZE];
                        serv_addr.sin_port = htons(PORT);
                        serv_addr.sin_addr.s_addr = inet_addr(dIPalias[i]);
                        int respuesta = connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        if (respuesta < 0) {
                                printf("%d", respuesta);
                                perror("[-] Error to connect");
                                close(client_sock);
                                return 1;
                        }
                        char clave[BUFFER_SIZE] = {0};
                        sendFile(archivo, clave); 
                        snprintf(mensaje, sizeof(mensaje), "%s", clave);
                        send(client_sock, mensaje, strlen(mensaje), 0);

                        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                        if (bytes <= 0) {
                                printf("[-] Missed key\n");
                                close(client_sock);
                                return 1;
                        }
                        buffer[bytes] = '\0';
                        printf("Encriptado: %s\n", buffer);
                        close(client_sock);
                }
        }
        return 0;
}
