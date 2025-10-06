#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Función para CIFRAR un texto usando el cifrado César
void encryptCaesar(char *text, int shift) {
    // Aseguramos que el desplazamiento esté entre 0 y 25
    shift = shift % 26;
    if (shift < 0) {
        shift += 26;
    }

    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            // (letra - 'A' + desplazamiento) % 26 + 'A'
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            // (letra - 'a' + desplazamiento) % 26 + 'a'
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        // Los caracteres que no son letras no se modifican
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s \"<texto a cifrar>\" <desplazamiento>\n", argv[0]);
        printf("Ejemplo: %s \"Hola Mundo\" 3\n", argv[0]);
        return 1;
    }

    char *text_to_encrypt = argv[1];
    int shift = atoi(argv[2]); // atoi convierte texto a número entero

    // Creamos una copia para no modificar el argumento original
    char text_copy[1024];
    strncpy(text_copy, text_to_encrypt, sizeof(text_copy) - 1);
    text_copy[sizeof(text_copy) - 1] = '\0';

    printf("Texto original: %s\n", text_copy);
    
    encryptCaesar(text_copy, shift);
    
    printf("Texto cifrado: %s\n", text_copy);

    return 0;
}
