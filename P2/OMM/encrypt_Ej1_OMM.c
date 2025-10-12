#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main() {
    char word[100];
    int shift;
    char input[10];

    // fgets lee la palabra
    printf("Enter word to encrypt: ");
    fgets(word, sizeof(word), stdin);
    
    // elimina el salto de linea que fgets incluye
    word[strcspn(word, "\n")] = '\0';

    // fgets lee el desplazamiento
    printf("Enter shift value: ");
    fgets(input, sizeof(input), stdin);
    shift = atoi(input);  // convierte la cadena a entero

    encryptCaesar(word, shift);
    printf("Encrypted: %s\n", word);

    return 0;
}

