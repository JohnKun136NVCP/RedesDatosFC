#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 7000
#define PORT 49200
/**
 * FUnción principal que obtiene el puerto a escuchar de la linea de comandos
 */
int main(int argc, char *argv[]) {
  if (argc != 2){
    printf("Type: %s <alias>\n",argv[0]);
    return 1;
  }
  char *server_ip = argv[1];
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
  server = gethostbyname(server_ip);
  server_addr.sin_addr = *((struct in_addr *)server->h_addr);
  
  if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))<0){
    perror("[-] Error binding");
    close(server_sock);
    return 1;
  }
  //Se comienza a eschucahr el puerto
  if (listen(server_sock, 5)<0){
    perror("[-] Error on listening");
    close(server_sock);
    return 1;
  }
  
  while(1){
    printf("[+] Server %s listening port %d... \n",server_ip,PORT);
    int client_sock;
    struct sockaddr_in client_addr;
    addr_size = sizeof(client_addr);
    //Se conecta con el cliente
    client_sock = accept(server_sock,(struct sockaddr *)&client_addr,&addr_size);
    if (client_sock < 0){
      perror("[-] Error on accept");
      close(client_sock);
      continue;
    }

    printf("[Server %s] Client connected\n",server_ip);

    char msg1 [50];
    int n;
    n = sprintf(msg1,"Servidor %s en espera",server_ip);
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
    n = sprintf(msg2,"Servidor %s : Nombre de archivo recibido",server_ip);
    send(client_sock,msg2,n,0);

    sleep(1);
    
    char buffer[BUFFER_SIZE] = {0};
    //Se obtiene el archivo
    bytes = recv(client_sock,buffer,sizeof(buffer),0);
    buffer[bytes] = '\0';
    char msg3 [50];
    n = sprintf(msg3,"Servidor %s : Procesando archivo",server_ip);
    send(client_sock,msg3,n,0);
    sleep(1);
    FILE *fpOutput;
    char *home_dir = getenv("HOME");
    char fname[512];
    snprintf(fname,sizeof(fname),"%s/%s/%s",home_dir,server_ip,name);
    printf("Servidor %s : Nombre del archivo: %s\n",server_ip,fname);
    fpOutput = fopen(fname,"w");
    if (fpOutput == NULL){
      perror("Error al guardar el archivo");
      char msg4 [50];
      n = sprintf(msg4,"Servidor %s Error al guardar el acrhivo",server_ip);
      send(client_sock,msg4,n,0);
      continue;
    }
    fputs(buffer,fpOutput);
    fclose(fpOutput);
    printf("Servidor %s : Archivo guardado en %s\n",server_ip,server_ip);
    char msg5 [50];
    n = sprintf(msg5,"Servidor %s: Archivo guardadp",server_ip);
    send(client_sock,msg5,n,0);
    close(client_sock);
  }
  close(server_sock);
  return 0;
}

  
  