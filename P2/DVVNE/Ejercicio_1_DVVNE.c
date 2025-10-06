#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//Encriptado Cesar
void cryptCaesar(char *text, int shift) {
        shift = shift % 26;
        for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                //De acuerdo al caracter del texto dado si se trata de una letra sumamos el desplazamiento dado, ya sea mayuscula o minuscula
                //si es algun otro tipo de simbolo no hacemos nada.
                if (isupper(c)) {
                        text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
                } else if (islower(c)) {
                        text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
                } else {
                        text[i] = c;
                }
        }
}

//El programa main recibe como argumento la plabra a encriptar y el desplazamiento
int main(int argc, char *argv[]){
        if(argc != 3){
                printf("USO: %s <palabra> <desplazamiento>\n", argv[0]);
                return 1;
        }

        char *clave = argv[1];
        char *shift = argv[2];
        //Si el dezplazamiento es igual a 0, ya sea porque no es un número o se paso un 0 en los argumentos, se envia un error.
        int desplazamiento = atoi(shift);
        if(desplazamiento == 0){
                printf("ERROR: Desplazamiento igual a 0\n");
                return 1;
        }
        else{
                //Si el desplazamiento es diferente de 0 regresamos la cadena encriptada.
                cryptCaesar(clave, desplazamiento);
                printf("%s\n", clave);
        }
        return 0;
}


