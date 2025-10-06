#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/select.h>

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

int max(int a, int b){
        return (a <= b)? b : a; 
}

int main(int argc, char *argv[]) {
        int conecciones[3];
        conecciones[0] = 49200;
        conecciones[1] = 49201;
        conecciones[2] = 49202;
        int server_sock[3], client_sock[3];
        //Creamos una estructura de datos de lectura para almacenar los sockets.
        fd_set reading;
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_size;
        char buffer[BUFFER_SIZE] = {0};
        char clave[BUFFER_SIZE];
        int shift;
        int pbuscado;
        //Estado de los sockets
        int rc;
        //Maximo
        int max_fd = 0;
        //Inicializamos la estructura
        FD_ZERO(&reading);
        //Crea un socket para cada puerto.
        for (int i = 0; i < 3; i++){
                server_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (server_sock[i] == -1) {
                        perror("[-] Error to create the socket");
                        return 1;
                }
        }
        //Configura la direccion del servidor (IPv4, puerto, cualquier IP local).
        server_addr.sin_family = AF_INET;
        for(int i = 0; i < 3; i++){
                //Asocia el socket al puerto correspondiente.
                server_addr.sin_port = htons(conecciones[i]);
                server_addr.sin_addr.s_addr = INADDR_ANY;
                //Pone el socket en modo escucha.
                if (bind(server_sock[i], (struct sockaddr *)&server_addr,sizeof(server_addr))< 0) {
                        perror("[-] Error binding");
                        close(server_sock[i]);
                        return 1;
                }
                if (listen(server_sock[i], 1) < 0) {
                        perror("[-] Error on listen");
                        close(server_sock[i]);
                        return 1;
                }
                printf("[*][CLIENT %d] LISTENING...\n", conecciones[i]);
        }
        //Usamos el While para que aun rechazada la conexión o no el servidor siga en escucha.
        while(1){
                //Guardammos cada socket en la estructura y calculamos el maximo.
                for(int i = 0; i < 3; i++){
                        FD_SET(server_sock[i], &reading); 
                        max_fd = max(max_fd, server_sock[i]);
                }
                rc = select(max_fd+1, &reading, NULL, NULL, NULL);
                if(rc <= 0){
                        perror( "select" );
                }else{
                        for(int i = 0; i < 3; i++){
                                if(FD_ISSET(server_sock[i], &reading)){
                                        //Espera y acepta una conexion entrante.
                                        addr_size = sizeof(client_addr);
                                        client_sock[i] = accept(server_sock[i], (struct sockaddr *)&client_addr,&addr_size);
                                        if (client_sock[i] < 0) {
                                                perror("[-] Error on accept");
                                                close(server_sock[i]);
                                                return 1;
                                        }       

                                        //Recibe el mensaje enviado por el cliente
                                        int bytes = recv(client_sock[i], buffer, sizeof(buffer) - 1, 0);
                                        if (bytes <= 0) {
                                                printf("[-] Missed key\n");
                                                close(client_sock[i]);
                                                close(server_sock[i]);
                                                return 1;
                                        }

                                        buffer[bytes] = '\0';
                                        //Extrae el puerto, la clave y el desplazamiento usando sscaf
                                        sscanf(buffer, "puerto: %d llave: %d contenido: %[^\n]", &pbuscado, &shift, clave); 
                                        //Si el puerto del servidor es igual al puerto buscado aplica el cifrado cesar sobre el contenido
                                        // del archivo y envia un mensaje de confirmación al cliente. 
                                        // Si no lo son, envia un mensaje de rechazo indicando que puerto se esta buscando.
                                        if(conecciones[i] == pbuscado){
                                                encryptCaesar(clave, shift);
                                                printf("[*][SERVER %d] Request accepted...\n", conecciones[i]);
                                                printf("[+][SERVER %d] File received and encrypted:\n %s\n", conecciones[i], clave);
                                                send(client_sock[i], "File received and encrypted", strlen("File received and encrypted"), 0);
                                        }else{
                                                send(client_sock[i], "REJECTED", strlen("REJECTED"), 0);
                                                printf("[*][SERVER %d] Request Rejected (Client request port %d)\n",conecciones[i], pbuscado);
                                        }
                                        close(client_sock[i]);
                                }
                        }
                }
        }
}

