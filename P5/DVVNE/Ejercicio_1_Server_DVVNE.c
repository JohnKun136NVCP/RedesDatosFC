#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

//Practica5

#define PORT 49200 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 //Tamaño del buffer
//Definimos los alias de cada una de nuestras ip's
char *dip[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103", "192.168.0.104"};
int numAlias = sizeof(dip)/sizeof(dip[0]);

//Usamos mutex para lograr la exclusión mutua y que dos sockets no traten de recibir o transmitir a la vez.
pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;
//Usamos cond para permitir que cualquiera de los sevidores suspenda su ejecución
pthread_cond_t condicion[sizeof(dip)/sizeof(dip[0])];

//Indica el servidor que esta en turno. El id esta dado por la posición en el arreglo
int turno = 0;

//Estructura para información del socket
typedef struct{
        int id;
        char *ipa;
        int client_sock, server_sock;
        //0 inicial, 1 cliente conectado, 2 recibio archivo. Si vuelve a 0 es que ya se completo todo el proceso y esta listo para recibir a otro cliente.
        int estado;
} informacionServer;

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

void *servidor(void *arg){
        informacionServer *infos = (informacionServer *)arg;
        int id = infos->id;
        char *ip = infos->ipa;

        struct sockaddr_in server_addr, client_addr;

        //Creamos el socket para el hilo
        infos->server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if ((infos->server_sock) == -1) {
                perror("[-] Error to create the socket");
                return NULL;
        }

        //Configura la direccion del servidor (IPv4, puerto, cualquier IP local).
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = inet_addr(ip);

        //Pone el socket en modo escucha.
        if (bind(infos->server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))< 0) {
                perror("[-] Error binding");
                close(infos->server_sock);
                return NULL;
        }

        if (listen(infos->server_sock, 1) < 0) {
                perror("[-] Error on listen");
                close(infos->server_sock);
                return NULL;
        }

        printf("[*][CLIENT %s:%d] LISTENING...\n", ip, PORT);

        //Configuramos el comportamiento del socket para que no se quede esperando al momento de conectar o recibir.
        fcntl(infos->server_sock, F_SETFL, O_NONBLOCK);

        while(1){
                char buffer[BUFFER_SIZE] = {0};
                char clave[BUFFER_SIZE];

                pthread_mutex_lock(&candado);
                //Mantenemos al hilo dormido hasta que sea su turno
                while(turno != id){
                        pthread_cond_wait(&condicion[id], &candado);
                } 

                //printf("Turno del servidor %s\n", ip);

                if(infos->estado == 0){
                        //Acepta al Cliente
                        socklen_t addr_size = sizeof(client_addr);
                        infos->client_sock = accept(infos->server_sock, (struct sockaddr *)&client_addr,&addr_size);
                        if(infos->client_sock >= 0){
                                infos->estado = 1;
                                printf("[*][CLIENT %s:%d] CONNECTION ACCEPTED...\n", ip, PORT);
                        }
                } else if (infos->estado == 1){
                        int bytes = recv(infos->client_sock, buffer, sizeof(buffer) - 1, 0);
                        if(bytes > 0){
                                buffer[bytes]= '\0';
                                //Extrae la clave usando sscaf
                                sscanf(buffer, "%[^\n]", clave);
                                infos->estado = 2;
                                printf("[*][CLIENT %s:%d] RECEIVED MESSAGE...\n", ip, PORT);
                        }
                } else if (infos->estado == 2){
                        encryptCaesar(clave, 32);
                        printf("[*][CLIENT %s:%d] SENDING MESSAGE...\n", ip, PORT);
                        send(infos->client_sock, clave, strlen(clave), 0);
                        close(infos->client_sock);
                        infos->estado = 0; 
                }

                turno = turno + 1;
                if(turno == numAlias){
                        turno = 0;
                }
                pthread_cond_signal(&condicion[turno]);
                pthread_mutex_unlock(&candado);

                sleep(1);
        }
}

int main(int argc, char *argv[]) {
        //Hilos
        pthread_t hilos[numAlias];
        informacionServer allInfo[numAlias];

        for(int i = 0; i < numAlias; i++){
                pthread_cond_init(&condicion[i], NULL);
                allInfo[i].id = i;
                allInfo[i].ipa = dip[i];
                allInfo[i].estado = 0;
                pthread_create(&hilos[i], NULL, servidor, &allInfo[i]);
        }

        pthread_cond_signal(&condicion[0]);
 
       for(int i = 0; i < numAlias; i++){
                pthread_join(hilos[i], NULL);
        }
}
