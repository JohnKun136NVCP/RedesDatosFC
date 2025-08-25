#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

// Función para aplicar el cifrado César
void encriptado_cesar(char *text, int shift) {
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isalpha(c)) {
            char base = isupper(c) ? 'A' : 'a';
            text[i] = (c - base + shift) % 26 + base;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <key> <shift>\n", argv[0]);
        return 1;
    }

    char key[BUFFER_SIZE];
    int shift = atoi(argv[2]);

    // Copiamos el mensaje a un buffer modificable
    strncpy(key, argv[1], BUFFER_SIZE - 1);
    key[BUFFER_SIZE - 1] = '\0';

    printf("Mensaje original: %s\n", key);

    // Ciframos
    encriptado_cesar(key, shift);

    printf("Mensaje cifrado : %s\n", key);

    return 0;
}
