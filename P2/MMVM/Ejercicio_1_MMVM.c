#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Función para cifrar 
void encryptCaesar(char *text, int shift) {
  shift = shift % 26; // desplazamiento de 0 a 25
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
    char text[100];
    int shift;

    printf("Ingrese el texto a cifrar: ");
    fgets(text, sizeof(text), stdin);
    text[strcspn(text, "\n")] = '\0'; 

    printf("Ingrese el valor de desplazamiento: ");
    scanf("%d", &shift);

    encryptCaesar(text, shift);

    printf("Texto cifrado: %s\n", text);

    return 0;
}

