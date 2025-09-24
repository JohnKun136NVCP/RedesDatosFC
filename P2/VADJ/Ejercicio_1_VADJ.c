#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
  char cadena[8] = {'p','a','r','a','c','m','e','\0'};
  cryptCaesar(cadena,46);
  printf("Cadena cifrada %s\n", cadena);
}
