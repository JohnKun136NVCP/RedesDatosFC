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
void conection(char *server_ip, char *obj_port, char *shift, char *file_name, int client_sock,
               struct sockaddr_in serv_addr,int port){
  char buffer[BUFFER_SIZE] = {0};
  char mensaje[BUFFER_SIZE];
  client_sock = socket(AF_INET, SOCK_STREAM, 0);// Se crea el socket
  if (client_sock == -1) {
    perror("[-] Error to create the socket");
    return;
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = inet_addr(server_ip);
  if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0) {//conectamos el socket al servidor
    perror("[-] Error to connect");
    close(client_sock);
    return ;
  }
  snprintf(mensaje, sizeof(mensaje), "%s %s", obj_port, shift);//Se configura el mensaje a enviar
  send(client_sock, mensaje, strlen(mensaje), 0);//Se envía el mensaje al servidor
  sendFile(file_name,client_sock);// Se envía el archivo indicado 
  int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
  if (bytes > 0) {
    buffer[bytes] = '\0';
    printf("[*] Server Response %d: %s\n",port, buffer);// Se imprime la respuesta del servidor
  } else {
    printf("[-] Server connection timeout\n");
  }
  close(client_sock);
}

/**
 * Función principal
 */
int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Type: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre del archivo>\n", argv[0]);
    return 1;
  }

  // Se obtienen los valores de la linea de comandos
  char *server_ip = argv[1];
  char *obj_port = argv[2];
  char *shift = argv[3];
  char *file_name = argv[4];

  // Se crean los sockets
  int client_sock;
  int client_sock2;
  int client_sock3;
  // direcciones de los servidores
  struct sockaddr_in serv_addr;
  struct sockaddr_in serv_addr2;
  struct sockaddr_in serv_addr3;
  // Se hacen las conexiones
  conection(server_ip,obj_port,shift,file_name,client_sock,serv_addr,PORT1);
  conection(server_ip,obj_port,shift,file_name,client_sock2,serv_addr2,PORT2);
  conection(server_ip,obj_port,shift,file_name,client_sock3,serv_addr3,PORT3);
  return 0;
}