#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

/**
 * FUnción que dado un String realiza la encriptación, según el cifrado cesar
 * REcibe el texto
 * Recibe el desplazamiento
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
 * FUnción principal que obtiene el puerto a escuchar de la linea de comandos
 */
int main(int argc, char *argv[]) {
  if (argc != 2){
    printf("Type: %s <PORT>\n",argv[0]);
    return 1;
  }
  int port = atoi(argv[1]);
  int server_sock,client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size;
  char buffer[BUFFER_SIZE] = {0};
  char clave[BUFFER_SIZE];
  char shift[BUFFER_SIZE];
  char re_port[BUFFER_SIZE];

  // Se crea el socket
  server_sock = socket(AF_INET,SOCK_STREAM,0);
  if (server_sock == -1){
    perror("[-] Error to create the socket");
    return 1;
  }

  // Se define el puerto
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))<0){
    perror("[-] Error binding");
    close(server_sock);
    return 1;
  }
  //Se comienza a eschucahr el puerto
  if (listen(server_sock, 1)<0){
    perror("[-] Error onn listen");
    close(server_sock);
    return 1;
  }
  
  printf("[+] Server listening port %d... \n",port);

  addr_size = sizeof(client_addr);
  //Se conecta con el cliente
  client_sock = accept(server_sock,(struct sockaddr *)&client_addr,&addr_size);
  if (client_sock < 0){
    perror("[-] Error on accept");
    close(server_sock);
    return 1;
  }

  printf("[+] Client connected\n");

  // Se obtiene la información del cliente
  int bytes = recv(client_sock,buffer,sizeof(buffer)-1,0);
  if (bytes <= 0){
    printf("[-] Missed key\n");
    close(client_sock);
    close(server_sock);
    return 1;
  }

  buffer[bytes] = '\0';
  sscanf(buffer, "%s %s",re_port,shift);
  // Se obtiene puerto y desplzamaiento del cliente
  printf("[+][Server] Se obtuvo puerto: %s y desplazamiento: %s \n",re_port,shift);
  if (strcmp(argv[1],re_port) == 0) {
    char caesar[BUFFER_SIZE];
    //Se obtiene el archivo
    bytes = recv(client_sock,caesar,sizeof(caesar),0);
    caesar[bytes] = '\0';
    // Se cifra
    encryptCaesar(caesar,atoi(shift));
    // Se imprime en pantalla el cifrado
    printf("[+][Server %d] File received and encrypted: %s",port,caesar);
    send(client_sock,"File received and encrypted",strlen("File received and encrypted"),0);
  } else {
    send(client_sock, "REJECTED",strlen("REJECTED"),0);
    printf("[-][SERVER] Wrong Port\n");
  }
  
  close(client_sock);
  close(server_sock);
  return 0;
}
