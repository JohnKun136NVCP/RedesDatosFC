#include <stdio.h>
#include <ctype.h>

/* Descifrar (mover hacia atrás) */
void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

/* Cifrar (mover hacia adelante) */
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main(void) {
    char texto[64] = "hola mundo";
    int shift = 10;

    encryptCaesar(texto, shift);
    printf("Texto cifrado:    %s\n", texto);

    decryptCaesar(texto, shift);
    printf("Texto descifrado: %s\n", texto);
    return 0;
}
