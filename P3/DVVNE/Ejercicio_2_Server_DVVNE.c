#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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
        int  puerto;
        int conecciones[3];
        conecciones[0] = 49200;
        conecciones[1] = 49201;
        conecciones[2] = 49202;
        if(argc < 2){
                puerto = PORT;
        } else {
                char *convertir = argv[1];
                puerto =  atoi(convertir);
                bool encontrado = 0;
                if(puerto == 0){
                        printf("ERROR: Puerto invalido letras\n");
                        return 1;
                }
                for(int i = 0; i < 3; i++){
                        if(puerto == conecciones[i]){
                                encontrado = 1;
                                break;
                        }
                }

                if(!encontrado){
                        printf("ERROR: Puerto invalido\n");
                        return 1;
                }
        }

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

                //Recibe el mensaje enviado por el cliente
                int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                        printf("[-] Missed key\n");
                        close(client_sock);
                        close(server_sock);
                        return 1;
                }

                buffer[bytes] = '\0';
                //Extrae el puerto, la clave y el desplazamiento usando sscaf
                sscanf(buffer, "puerto: %d llave: %d contenido: %[^\n]", &pbuscado, &shift, clave); 
                //Si el puerto del servidor es igual al puerto buscado aplica el cifrado cesar sobre el contenido
                // del archivo y envia un mensaje de confirmación al cliente. 
                // Si no lo son, envia un mensaje de rechazo indicando que puerto se esta buscando.
                if(puerto == pbuscado){
                        encryptCaesar(clave, shift);
                        printf("[*][SERVER %d] Request accepted...\n", puerto);
                        printf("[+][SERVER %d] File received and encrypted:\n %s\n", puerto, clave);
                        send(client_sock, "File received and encrypted", strlen("File received and encrypted"), 0);
                        //Libera los recursos de red.
                        close(client_sock);
                }else{
                        send(client_sock, "REJECTED", strlen("REJECTED"), 0);
                        printf("[*][SERVER %d] Request Rejected (Client request port %d)\n",puerto, pbuscado);
                }
        }
}
