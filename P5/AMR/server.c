#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <poll.h>

#define BUFFER_SIZE 7000
#define PORT 49200

//Estructura para mandar al hilo
struct SocketInfo{
  char *ip;
  int server;
  int sock;
};


// Variables globales para manejar los hilos
int turno = 0;
pthread_mutex_t turnos;
pthread_cond_t disponible;

/**
 *FUnción que ejecuta el algoritmo RoundRobin sirviendo a clientes
 *Ea la función que se ejecuta en cada hilo
 */
void *roundRobin(void *serverInfo){
  //printf("AAAAAA\n");
  struct SocketInfo *server = (struct SocketInfo *)serverInfo;

  //Comienza el algoritmo Roun RObin, se tiene que bloquear el turno actual y garantizar que es su turno
  while (1){
  	//printf("Eseprando %s\n",server->ip);
    // PAra poder bloquear cuando no es el turno del servidor se siguió lo siguiente:
    // https://stackoverflow.com/questions/16522858/understanding-of-pthread-cond-wait-and-pthread-cond-signal
    pthread_mutex_lock(&turnos);
    //Se verifica que sea el turno del servidor
    while (turno != server->server){
      pthread_cond_wait(&disponible,&turnos);
      //printf("DEntro de cond %s\n",server->ip);
    }
    //printf("Round Robin turno de: %s\n",server->ip);
    //Se verá si hay un cliente en espera a ser atendido
    printf("Round Robin turno de: %s, escuchando en el puerto: %d\n",server->ip,PORT);
    struct pollfd pfd;
    pfd.fd = server->sock;
    pfd.events = POLLIN;
    int quantum = 1500;
    int answer = poll(&pfd,1,quantum);
    if (answer < 0){
      perror("poll failed'n");
    } else if (answer == 0){
      printf("Servidor %s no recibió cliente\n",server->ip);
    } else{
    	if (pfd.revents & POLLIN){
        pthread_mutex_unlock(&turnos);
        int client_sock;
        struct sockaddr_in client_addr;
        socklen_t addr_size;
        addr_size = sizeof(client_addr);
        //Se conecta con el cliente
        client_sock = accept(server->sock,(struct sockaddr *)&client_addr,&addr_size);
        if (client_sock < 0){
          perror("[-] Error on accept");
          close(client_sock);
          continue;
        }
        printf("[Server %s] Client connected\n",server->ip);

        char msg1 [50];
        int n;
        n = sprintf(msg1,"Servidor %s en espera",server->ip);
        send(client_sock,msg1,n,0);

        sleep(1);

        char filename[512] = {0};
        int bytes = recv(client_sock,filename,sizeof(filename)-1,0);
        if (bytes <= 0){
          printf("Error al recibir\n");
          close(client_sock);
          continue;
        }
        filename[bytes] = '\0';
        char name[512];
        sscanf(filename,"%s",name);

        char msg2 [50];
        n = sprintf(msg2,"Servidor %s : Nombre de archivo recibido",server->ip);
        send(client_sock,msg2,n,0);
        sleep(1);
        char buffer[BUFFER_SIZE] = {0};
        //Se obtiene el archivo
        bytes = recv(client_sock,buffer,sizeof(buffer),0);
        buffer[bytes] = '\0';
        char msg3 [50];
        n = sprintf(msg3,"Servidor %s : Procesando archivo",server->ip);
        send(client_sock,msg3,n,0);
        sleep(1);
        FILE *fpOutput;
        char *home_dir = getenv("HOME");
        char fname[512];
        snprintf(fname,sizeof(fname),"%s/%s/%s",home_dir,server->ip,name);
        printf("Servidor %s : Nombre del archivo: %s\n",server->ip,fname);
        fpOutput = fopen(fname,"w");
        if (fpOutput == NULL){
          perror("Error al guardar el archivo");
          char msg4 [50];
          n = sprintf(msg4,"Servidor %s Error al guardar el archivo",server->ip);
          send(client_sock,msg4,n,0);
          continue;
        }
        fputs(buffer,fpOutput);
        fclose(fpOutput);
        printf("Servidor %s : Archivo guardado en %s\n",server->ip,server->ip);
        char msg5 [50];
        n = sprintf(msg5,"Servidor %s: Archivo guardadp",server->ip);
        send(client_sock,msg5,n,0);
        close(client_sock);
      }
    }
    turno = turno == 0 ? 1 : turno == 1 ? 2 : turno == 2 ? 3 : 0;
    //pthread_cond_signal(&disponible);
    // Se decidió por usar broadcast para obligar a un hilo a pasar del candado, pues con signal se tenía un deadlock
    // https://www.ibm.com/docs/en/aix/7.2.0?topic=p-pthread-cond-signal-pthread-cond-broadcast-subroutine
    pthread_cond_broadcast(&disponible);
    pthread_mutex_unlock(&turnos);
  }
  return 0;
}

/**
 * FUnción principal
 */
int main(int argc, char *argv[]) {
  if (argc != 1){
    printf("Type: %s\n",argv[0]);
    return 1;
  }
  pthread_mutex_init(&turnos,NULL);
  pthread_cond_init(&disponible,NULL);
  char *alias[] = {"s01","s02","s03","s04"};
  // Se sigió el siguiente ejemplo
  // https://stackoverflow.com/questions/35061854/creating-multiple-threads-in-c
  // Esto para poder crear los hilos dentro de un for se utiliza un arreglo
  pthread_t hilos[4];
  struct SocketInfo *info;
  for (int i = 0; i<4;i++){
    int server_sock;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    // Se crea el socket
    server_sock = socket(AF_INET,SOCK_STREAM,0);
    if (server_sock == -1){
      perror("[-] Error to create the socket");
      return 1;
    }

    // Se define el puerto
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    //server_addr.sin_addr.s_addr = inet_addr("192.168.0.101");
    //server_addr.sin_addr.s_addr = INADDR_ANY;
    //server_addr.sin_addr.s_addr = server_ip;
    struct hostent *server;
    server = gethostbyname(alias[i]);
    server_addr.sin_addr = *((struct in_addr *)server->h_addr);
  

    if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))<0){
      perror("[-] Error binding");
      close(server_sock);
      return 1;
    }
    // Se sigió lo siguiente para pooder mandar una estrcutura al hilo
    // https://stackoverflow.com/questions/20196121/passing-struct-to-pthread-as-an-argument

    info = malloc(sizeof(struct SocketInfo));
    (*info).ip = alias[i];
    (*info).server = i;
    (*info).sock = server_sock;
    //Se comienza a eschucahr el puerto
    if (listen(server_sock, 5)<0){
      perror("[-] Error on listening");
      close(server_sock);
      return 1;
    }
    //Se crea el hilo para recibir los clientes
    pthread_create(&hilos[i],NULL,&roundRobin,(void*) info);
    printf("hilo %d creado\n",i);
  }
  //Se espera a que terminen de ejecutarse los hilos
  pthread_join(hilos[0],NULL);
  pthread_join(hilos[1],NULL);
  pthread_join(hilos[2],NULL);
  pthread_join(hilos[3],NULL);

  pthread_mutex_destroy(&turnos);
  pthread_cond_destroy(&disponible);
  return 0;
}