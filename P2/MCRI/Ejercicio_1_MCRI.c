#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Función para cifrar el texto usando el cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) { 
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        } 
        else if (islower(c)) { 
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        } 
        else { text[i] = c;
        } 
    } 
}