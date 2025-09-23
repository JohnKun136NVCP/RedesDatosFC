#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT1 49200
#define PORT2 49201
#define PORT3 49202
#define BUFFER_SIZE 1024

/**
 * Funcion que envía el archivo
 */
void sendFile(const char *filename, int sockfd){
  FILE *fp = fopen(filename,"r");
  if (fp == NULL){
    perror("[-] Cannot open the file");
    return;
  }
  char buffer[BUFFER_SIZE];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer),fp)) >0){
    if (send(sockfd, buffer, bytes,0) == -1){
      perror("[-] Error to send file");
      break;
    }
  }
  fclose(fp);
}

/**
 * Esta función es idéntica a la de client.c
 * Función que realiza la conexión entre cliente-servidor y manda el archivo leido
 * Recibe la IP del servidor
 * El puerto objetivo
 * EL desplazamiento
 * El nombre del archivo a mandar
 * el socket del cliente
 * la dirección del servidor
 * el puerto del servidor
 */
void conection(char *server_ip, char *obj_port, char *shift, char *file_name, int client_sock,
               struct sockaddr_in serv_addr,int port){
  char buffer[BUFFER_SIZE] = {0};
  char mensaje[BUFFER_SIZE];
  client_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (client_sock == -1) {
    perror("[-] Error to create the socket");
    return;
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = inet_addr(server_ip);
  if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {
    perror("[-] Error to connect");
    close(client_sock);
    return ;
  }
  snprintf(mensaje, sizeof(mensaje), "%s %s", obj_port, shift);
  send(client_sock, mensaje, strlen(mensaje), 0);
  sendFile(file_name,client_sock);
  int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
  if (bytes > 0) {
    buffer[bytes] = '\0';
    printf("[*] Server Response %d: %s\n",port, buffer);
  } else {
    printf("[-] Server connection timeout\n");
  }
  close(client_sock);
}

/**
 * Función principal
 */
int main(int argc, char *argv[]) {
  // El formato para la línea de comandos
  if (argc < 5) {
    printf("Type: %s <SERVIDOR_IP> <PUERTO1>... <Nombre del archivo1>... <DESPLAZAMIENTO>\n", argv[0]);
    return 1;
  }
  //Se tiene que brindar un puerto
  int conexiones = argc-3;
  // Se comprueban los valores
  if (conexiones % 2 != 0){
    printf("Se requiere el mismo número de puertos que archivos a leer\n");
    return 1;
  }
  //Valores para todos los sockets
  char *server_ip = argv[1];
  char *shift = argv[argc-1];
  int puertos = conexiones/2;
  // Se recorren los sockets y archivos y se realizan las conexiones
  for (int i = 2; i<2+puertos; i++){
    char *obj_port = argv[i];
    int port = atoi(obj_port);
    char *file_name = argv[i+puertos];
    int client_sock;
    struct sockaddr_in serv_addr;
    //Conexión con el servidor
    conection(server_ip,obj_port,shift,file_name,client_sock,serv_addr,port);
  }
  return 0;
}