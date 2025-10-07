#include <stdio.h>
#include <ctype.h>

// Función para cifrar con César (desplazamiento hacia adelante)
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        // Si no es letra, no lo cambia
    }
}

int main() {
    char text[1024];
    int shift;

    printf("Ingrese el texto a cifrar: ");
    fgets(text, sizeof(text), stdin);

    printf("Ingrese el desplazamiento (shift): ");
    scanf("%d", &shift);

    encryptCaesar(text, shift);

    printf("Texto cifrado: %s\n", text);

    return 0;
}
