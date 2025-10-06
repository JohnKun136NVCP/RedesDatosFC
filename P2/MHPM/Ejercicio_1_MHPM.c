#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/*
Código para cifrar por medio del método César
*/

/* para esto podemos notar que usamos parte del código que se dio en la práctica, esta parte 
lo que hace es ENCRIPTAR la palabra que le damos
*/
void encryptCaesar(char *text, int shift) {
    shift = shift % 26; 
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            // En esta parte aplica el cifrado a las letras mayúculas 
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            // En esta parte aplica el cifrado a las letras minúsculas 
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }else {
            text[i] = c;
       }
    }
}

/*
Hacemos el main donde se le pedirá al usuario la cadena que 
quiere encriptar y el desplazamiento que le quiere dar
*/

int main() {
    char text[100];
    int shift;

    printf("Ingresa la cadena que quieres encriptar: ");
    fgets(text, sizeof(text), stdin);
    text[strcspn(text, "\n")] = '\0'; 

    printf("Ingresa el desplazamiento: ");
    scanf("%d", &shift);

    encryptCaesar(text, shift);
    printf("Cadena encriptada: %s\n", text);

    return 0;
}