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
        //printf("\n %s \n", filename);
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
        //Recibimos la direccion ip del servidor
        char *server_ip = argv[1];
        //Guardamos los puertos dados por el usuario, el ciclo se rompe cuando se 
        // encuentra algo que no pueda convertirse a numero o superemos los 3 puertos
        int puerto[3];
        int i = 0;
        while (i < 3){
                int conversionp = atoi(argv[2+i]);
                if(conversionp != 0){
                        puerto[i] = conversionp; 
                }
                else{
                        break;
                }
                i++;
        }
        //Sumamos dos pues es donde en el arreglo argv se dan los puertos
        i += 2;
        char *archivos[3] = {NULL, NULL, NULL};
        int num_archivos = 0;
        //Leemos los archivos, se tiene un limite de 3. Uno para cada servidor.
        for (int j = i; j < argc-1 && num_archivos < 3; j++){
                archivos[num_archivos++] = argv[j];
        }
        //Si no se dan archivos mandamos un error.
        if(archivos[0]== NULL){
                printf("No se dieron archivos.");
                return 1;
        } 
        //Si solo se da un archivo lo copiamos en los otros espacios del arreglo
        else if(archivos[1] == NULL){
                archivos[1] = archivos[0];
        }
        //Si se dan solo dos archivos, copiamos el de posicion uno en la ultima posicion
        if(archivos[2]== NULL){
                archivos[2] = archivos[1];
        }
        char *shift = argv[argc-1];
        int desplazamiento = atoi(shift);
        if(desplazamiento == 0){
                printf("Desplazamiento Invalido");
                return 1;
        }

        int client_sock[3]; 
        struct sockaddr_in serv_addr;
        for (int i = 0; i < (sizeof(client_sock)/sizeof(client_sock[0])); i++){
                client_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (client_sock[i] == -1) {
                        perror("[-] Error to create the socket");
                        return 1;
                }
        }
        serv_addr.sin_family = AF_INET; 
        //Hacemos una conexion a los puertos mandados, donde si un indice de los puertos es igual a 0 lo saltamos.
        // Se supone que los archivos dados estan en el orden respectivo de los  puertos dados.
        for (int i = 0; i < (sizeof(puerto)/sizeof(puerto[0])); i++){ 
                if(puerto[i] == 0){
                        break;
                }
                char buffer[BUFFER_SIZE] = {0};
                char mensaje[BUFFER_SIZE];
                serv_addr.sin_port = htons(puerto[i]);
                serv_addr.sin_addr.s_addr = inet_addr(server_ip);
                int respuesta = connect(client_sock[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                if (respuesta < 0) {
                        printf("%d", respuesta);
                        perror("[-] Error to connect");
                        close(client_sock[i]);
                        break;
                }

                char clave[BUFFER_SIZE] = {0};
                sendFile(archivos[i], clave); 
                snprintf(mensaje, sizeof(mensaje), "puerto: %d llave: %d contenido: %s", puerto[i], desplazamiento, clave);
                send(client_sock[i], mensaje, strlen(mensaje), 0);

                int bytes = recv(client_sock[i], buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                        buffer[bytes] = '\0';
                        printf("[*]SERVER  RESPONSE %d: %s\n",puerto[i], buffer);
                } else {
                        printf("[-] Server connection tiemeout\n");
                }
                close(client_sock[i]);
        }
        return 0;
}
