#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Descifrado visto en clase
void decryptCaesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' - shift + 26) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' - shift + 26) % 26) + 'a');
        }
    }
}

// Cifrado 
void encryptCaesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        }
    }
}

// Prueba
int main(void) {
    char texto[50] = "Zorro";
    int desplazamiento = 7;

    encryptCaesar(texto, desplazamiento);
    printf("Texto cifrado: %s\n", texto);

    decryptCaesar(texto, desplazamiento);
    printf("Texto descifrado: %s\n", texto);

    return 0;
}