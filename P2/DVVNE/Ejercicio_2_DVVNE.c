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

//El programa main recibe como argumento el archivo a leer y el desplazamiento
int main(int argc, char *argv[]){
        if(argc != 3){
                printf("USO: %s <archivo> <desplazamiento>\n", argv[0]);
                return 1;
        }

        char *archivo = argv[1];
        char *shift = argv[2];
        //Si el desplazamiento son iguales a 0 se envia un error. 
        int desplazamiento = atoi(shift);
        if(desplazamiento == 0){
                printf("ERROR: Desplazamiento igual a 0\n");
                return 1;
        }
        else{
                //Si son diferentes, encriptamos los primeros n caracteres del archivo dado.
                char buffer[1024];
                //Abrimos el archivo.
                FILE *canal;
                canal = fopen(archivo, "r");
                if(canal == NULL){
                        printf("File error");
                        return 1;
                }
                //Encriptamos
                while (fgets(buffer, sizeof(buffer), canal) != NULL){
                        //Imprimimos
                        cryptCaesar(buffer, desplazamiento);
                        printf("%s\n", buffer);
                }
                //Cerramos el archivo
                if(fclose(canal)){
                        perror("fclose error");
                }
        }
        return 0;
}
