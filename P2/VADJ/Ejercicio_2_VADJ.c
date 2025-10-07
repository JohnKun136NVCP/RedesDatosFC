#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void cryptCaesar(char *text, int shift){
  shift = shift % 26;
  for (int i = 0; text[i] !=  '\0'; i++){
    char c = text[i];
    if(isupper(c)){
      text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
    } else if (islower(c)) {
      text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
    } else {
      text[i] = c;
    }
  }
}

int main(){
  char buffer[100];
  char clave[100];
  int b = 0;
  printf("Ingrese la cadena a cifrar:\n");
  fgets(buffer,100,stdin);
  for(int i = 0; i < 100; i++){
    if (buffer[i] == '\n'){
      buffer[i] = '\0';
      break;
    }
  }
  printf("Ingrese la clave:\n");
  fgets(clave, 100, stdin);
  for(int i = 0; i < 100; i++){
    if(clave[i] == '\n') {
      clave[i] = '\0';
      break;
    }
  }
  b = atoi(clave);
  cryptCaesar(buffer, b);
  printf("La cadena cifrada es: %s\n", buffer);
  
}
