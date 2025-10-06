// Versión de Cifrado César (tome la función del descifrado pues solo era necesario cambiar signo)
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// Función para cifrar usando César
void encryptCesar(char *text, int shift) {

    shift = ((shift % 26) + 26) % 26;

    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];

        if (isupper(c)) {
            // Mayúsculas entonces desplazamos hacia adelante
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            // Minúsculas entonces desplazamos hacia adelante
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            // Caracteres que no son letras se dejan igual
            text[i] = (char)c;
        }
    }
}

int main(void) {
    char text[1024];
    int key;

    printf("Texto a cifrar: ");
    if (!fgets(text, sizeof(text), stdin)) {
        fprintf(stderr, "Error leyendo texto\n");
        return 1;
    }
    // Quitamos el salto de línea si viene al final
    size_t n = strlen(text);
    if (n > 0 && text[n - 1] == '\n') text[n - 1] = '\0';

    printf("Key (entero): ");
    if (scanf("%d", &key) != 1) {
        fprintf(stderr, "Key inválido\n");
        return 1;
    }
    //Ciframos y retornamos el texto
    encryptCesar(text, key);
    printf("Texto cifrado: %s\n", text);

    return 0;
}
