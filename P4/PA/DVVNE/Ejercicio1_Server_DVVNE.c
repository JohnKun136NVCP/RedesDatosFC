#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include<time.h>

//Practica4

#define PORT 49200 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 //Tamaño del buffer

void encryptCaesar(char *text, int shift) {
        shift = shift % 26;
        //Para cada caracter en la cadena dada, si se trata de una letra ya sea mayuscula
        //o minuscula lo mueve hacia atrás según el cifrado cesar. Si no corresponde a una letra, lo deja tal y como esta.
        for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                if (isupper(c)) {
                        text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
                } else if (islower(c)) {
                        text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
                } else {
                        text[i] = c;
                }
        }
}

int main(int argc, char *argv[]) {
        char *dip =  "192.168.0.101";
        if(argc > 1){   
               if(strcmp(argv[1], "s01") == 0){
                        dip = "192.168.0.101";
                }
                else if(strcmp(argv[1], "s02") == 0){
                        dip = "192.168.0.102";
                }
        }
        //printf("%s", dip);
        int  puerto;
        int conecciones = 0;
        puerto = PORT;
        int server_sock, client_sock;
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_size;
        char buffer[BUFFER_SIZE] = {0};
        char clave[BUFFER_SIZE];
        int shift;
        int pbuscado;
        //Crea un socket.
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == -1) {
                perror("[-] Error to create the socket");
                return 1;
        }
        //Configura la direccion del servidor (IPv4, puerto, cualquier IP local).
        server_addr.sin_family = AF_INET;
        //Asocia el socket al puerto.
        server_addr.sin_port = htons(puerto);
        //server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_addr.s_addr = inet_addr(dip);
        printf("IP: %s\n", dip);
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
        printf("[*][CLIENT %d] LISTENING...\n", puerto);
        //Usamos el While para que aun rechazada la conexión o no el servidor siga en escucha.
        while(1){
                //Espera y acepta una conexion entrante.
                addr_size = sizeof(client_addr);
                client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
                if (client_sock < 0) {
                        perror("[-] Error on accept");
                        close(server_sock);
                        return 1;
                }

                //Al aceptar al cliente aumentamos en uno el número de conecciones.
                conecciones += 1;
                char mensaje[BUFFER_SIZE];
                int pnuevo = puerto+conecciones;
                //Creamos un nuevo socket con el puerto a enviar.
                int serverNew, clientNew;
                serverNew = socket(AF_INET, SOCK_STREAM, 0);
                if (serverNew == -1) {
                        perror("[-] Error to create the socket");
                        return 1;
                }

                server_addr.sin_addr.s_addr = inet_addr(dip);
                server_addr.sin_port = htons(pnuevo);

                if (bind(serverNew, (struct sockaddr *)&server_addr,sizeof(server_addr))< 0) {
                        perror("[-] Error binding");
                        close(serverNew);
                        return 1;
                }
                if (listen(serverNew, 1) < 0) {
                        perror("[-] Error on listen");
                        close(serverNew);
                        return 1;
                }
                //Enviamos el nuevo puerto a conectarse
                snprintf(mensaje, sizeof(mensaje), "puerto: %d", pnuevo);
                send(client_sock, mensaje, strlen(mensaje), 0);

                clientNew = accept(serverNew, (struct sockaddr *)&client_addr,&addr_size);
                if (clientNew < 0) {
                        perror("[-] Error on accept");
                        close(serverNew);
                        return 1;
                }
                //printf("Aceptamos Puerto Nuevo \n");
                time_t curtime;
                time(&curtime);
                snprintf(mensaje, sizeof(mensaje), "%s Estado: Esperando... Puerto:%d", ctime(&curtime), pnuevo);
                send(clientNew, mensaje, strlen(mensaje), 0);

                //Recibe el mensaje enviado por el cliente
                int bytes = recv(clientNew, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                        printf("[-] Missed key\n");
                        close(clientNew);
                        close(serverNew);
                        return 1;
                }

                buffer[bytes] = '\0';
                //Extrae el puerto, la clave y el desplazamiento usando sscaf
                sscanf(buffer, "%[^\n]", clave);

                //Recibbimos archivo
                snprintf(mensaje, sizeof(mensaje), "%s Estado: Recibiendo archivo... Puerto:%d", ctime(&curtime), pnuevo);
                send(clientNew, mensaje, strlen(mensaje), 0);
                sleep(1);
                //Enviamos archivo ya cifrado           
                encryptCaesar(clave, 32);
                snprintf(mensaje, sizeof(mensaje), "%s Estado: Enviando archivo... Puerto:%d", ctime(&curtime), pnuevo);
                send(clientNew, mensaje, strlen(mensaje), 0);
                sleep(1);
                snprintf(mensaje, sizeof(mensaje), "%s", clave);
                send(clientNew, mensaje, strlen(mensaje), 0);

                close(clientNew); 
        }
}
