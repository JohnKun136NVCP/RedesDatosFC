#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
// Reusa el descifrador invirtiendo el signo del desplazamiento por lo que 
// se mueve hacia adelante en lugar de hacia atrás
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {text[i] = ((c - 'A' + shift +26) % 26) + 'A'; 
        }else if (islower(c)) {text[i] = ((c - 'a' + shift +26) % 26) + 'a';
    } else {
        text[i] = c; 
    }
}
}



