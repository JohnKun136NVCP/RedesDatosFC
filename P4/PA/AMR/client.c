#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT1 49200
#define BUFFER_SIZE 7000

/*
*Función para escribir eventos en la bitácora.
*/
void event_log(const char *event){
  FILE *f = fopen("log.txt","a");
  if (f == NULL){
    perror("Error al abrir la bitácora");
    return;
  }
  time_t now = time(NULL);
  char times[64];
  strftime(times,sizeof(times),"%Y-%m-%d %H:%M:%S",localtime(&now));
  fprintf(f,"%s - %s\n",times,event);
  fclose(f);
  printf("%s - %s\n",times,event);
}

/*Función para enviar un archivo
*Recibe el nombre del archivo y el descriotor del socket
*
*/
void sendFile(const char *filename, int sockfd){
  FILE *fp = fopen(filename,"r"); // Se abre el archivo
  if (fp == NULL){
    perror("[-] Cannot open the file");
    return;
  }
  char buffer[BUFFER_SIZE];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer),fp)) >0){
    if (send(sockfd, buffer, bytes,0) == -1){ // Se manda el archivo
      perror("[-] Error to send file");
      break;
    }
  }
  fclose(fp);
}

/**
 * Función que realiza la conexión entre cliente-servidor y manda el archivo leido
 * Recibe la IP del servidor
 * El puerto objetivo
 * EL desplazamiento
 * El nombre del archivo a mandar
 * el socket del cliente
 * la dirección del servidor
 * el puerto del servidor
 */
void conection(char *server_ip, char *puerto, char *file_name, int client_sock,
               struct sockaddr_in serv_addr){
  char buffer[BUFFER_SIZE] = {0};
  char mensaje[512];
  char mensaje1[512]={0};
  char mensaje2[512]={0};
  char mensaje3[512]={0};
  char mensaje4[512]={0};
  int port = atoi(puerto);
  client_sock = socket(AF_INET, SOCK_STREAM, 0);// Se crea el socket
  if (client_sock == -1) {
    perror("[-] Error to create the socket");
    return;
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  struct hostent *server;
  server = gethostbyname(server_ip);
  serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
  //serv_addr.sin_addr.s_addr = inet_addr(server_ip);
  if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {//conectamos el socket al servidor
    perror("[-] Error to connect");
    close(client_sock);
    return ;
  }
  int bytes = recv(client_sock,mensaje1,sizeof(mensaje1)-1,0);
  if (bytes <= 0){
    perror("No se recibió el estado del servidor");
    close(client_sock);
    return;
  }
  mensaje1[bytes] = '\0';
  event_log(mensaje1);
  
  snprintf(mensaje, sizeof(mensaje), "%s",file_name);//Se configura el mensaje a enviar
  send(client_sock,mensaje,strlen(mensaje),0);
  sleep(1);

  bytes = recv(client_sock,mensaje2,sizeof(mensaje2)-1,0);
  if (bytes <= 0){
    perror("No se recibió confirmación del servidor");
    close(client_sock);
    return;
  }
  mensaje2[bytes] = '\0';
  event_log(mensaje2);

  sendFile(file_name,client_sock);// Se envía el archivo indicado
  sleep(1);
  
  bytes = recv(client_sock, mensaje3, sizeof(mensaje3) - 1, 0);
  if (bytes <= 0) {
    perror("No se recibió estado del proceso del servidor");
    close(client_sock);
    return;
  }
  mensaje3[bytes] = '\0';
  event_log(mensaje3);

  sleep(3);
  bytes = recv(client_sock, mensaje4, sizeof(mensaje4) - 1, 0);
  if (bytes <= 0) {
    perror("No se recibió estado del archivo del servidor");
    close(client_sock);
    return;
  }
  mensaje4[bytes] = '\0';
  event_log(mensaje4);
  event_log("Finalizado");
  close(client_sock);
}

/**
 * Función principal
 */
int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Type: %s <SERVIDOR_IP> <PORT>  <Nombre del archivo>\n", argv[0]);
    return 1;
  }

  // Se obtienen los valores de la linea de comandos
  char *server_ip = argv[1];
  char *port = argv[2];
  char *file_name = argv[3];

  // Se crean los sockets
  int client_sock;
  // direcciones de los servidores
  struct sockaddr_in serv_addr;
  // Se hacen las conexiones
  conection(server_ip,port,file_name,client_sock,serv_addr);
  return 0;
}


  