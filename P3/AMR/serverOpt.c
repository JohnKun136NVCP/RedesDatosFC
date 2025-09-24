#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024


/**
 * Función que realiza el cifrado Cesar
 */
void encryptCaesar(char *text, int shift){
  shift = shift % 26;
  for (int i = 0; text[i] != '\0';i++){
    char c = text[i];
    if (isupper(c)){
      text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
    } else if (islower(c)){
      text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
    } else {
      text[i] = c;
    }
  }
}

/**
 * FUnción que realiza el funcionamiento del servidor.
 * Su comportamiento es idéntico al Main del archivo server.c
 * Recibe un apuntador al puerto a escuchar
 */
void* serve(void* arg){
  int port = *(int*)arg;
  int server_sock,client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size;
  char buffer[BUFFER_SIZE] = {0};
  char clave[BUFFER_SIZE];
  char shift[BUFFER_SIZE];
  char re_port[BUFFER_SIZE];

  server_sock = socket(AF_INET,SOCK_STREAM,0);
  if (server_sock == -1){
    perror("[-] Error to create the socket");
    return 0;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))<0){
    perror("[-] Error binding");
    close(server_sock);
    return 0;
  }

  if (listen(server_sock, 1)<0){
    perror("[-] Error onn listen");
    close(server_sock);
    return 0;
  }

  printf("[+] Server listening port %d... \n",port);
  
  addr_size = sizeof(client_addr);
  client_sock = accept(server_sock,(struct sockaddr *)&client_addr,&addr_size);
  if (client_sock < 0){
    perror("[-] Error on accept");
    close(server_sock);
    return 0;
  }

  //printf("[+] Client connected\n");

  int bytes = recv(client_sock,buffer,sizeof(buffer)-1,0);
  if (bytes <= 0){
    printf("[-] Missed key\n");
    close(client_sock);
    close(server_sock);
    return 0;
  }

  buffer[bytes] = '\0';
  sscanf(buffer, "%s %s",re_port,shift);
  //printf("[+][Server %d] Se obtuvo puerto: %s y desplazamiento: %s \n",port,re_port,shift);
  if (port == atoi(re_port)) {
    char caesar[BUFFER_SIZE];
    bytes = recv(client_sock,caesar,sizeof(caesar),0);
    caesar[bytes] = '\0';
    encryptCaesar(caesar,atoi(shift));
    FILE *fpOutput;
    char name[50];
    sprintf(name,"file_%d_cesar.txt",port);
    fpOutput = fopen(name,"w");
    if (fpOutput == NULL){
      perror("[-] Error al crear archivos");
      return 0;
    }
    fputs(caesar,fpOutput);
    fclose(fpOutput);
    printf("[+][Server %d] File received and encrypted to file: %s\n",port,name);
    send(client_sock,"File received and encrypted",strlen("File received and encrypted"),0);
  } else {
    send(client_sock, "REJECTED",strlen("REJECTED"),0);
    printf("[-][SERVER] Wrong Port\n");
  }


  close(client_sock);
  close(server_sock);
  return 0;
}

/**
 * Función principal que crea los hilos de ejecución
 */
int main(int argc, char *argv[]) {
  if (argc != 1){
    printf("Type: %s \n",argv[0]);
    return 1;
  }
  //Puertos a escuchar
  int puerto1 = 49200;
  int puerto2 = 49201;
  int puerto3 = 49202;
  // Se crean los hilos 
  pthread_t hilo1;
  pthread_t hilo2;
  pthread_t hilo3;
  //Se ejecutan los hilos de ejecución con la función para funcionar como servidor y el puerto asociado
  pthread_create(&hilo1,NULL,&serve,&puerto1);
  pthread_create(&hilo2, NULL,&serve,&puerto2);
  pthread_create(&hilo3, NULL,&serve,&puerto3);

  // Se espera a que terminen los hilos
  pthread_join(hilo1,NULL);
  pthread_join(hilo2,NULL);
  pthread_join(hilo3,NULL);

  return 0;
}
