#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void funcion (){
  char mem[50];
  printf("ingresa algo por texto \n");
  fgets(mem,sizeof(mem), stdin);
  printf("lo que escribiste es: ");
  printf(mem);
}


int main (){
  funcion();
  return 0;
}
