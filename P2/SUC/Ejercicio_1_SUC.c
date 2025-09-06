#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tamaño máximo de la línea a leer con fgets (incluye el byte del '\0')
#define MAX_LINE_LENGTH 1024

/*
 * Cifra el texto en sitio usando el cifrado César.
 * - text: cadena a modificar (terminada en '\0').
 * - shift: desplazamiento (puede ser negativo o mayor que 26).
 */
void encryptCaesar(char *text, int shift) {
    // Si no hay texto, no hay nada que cifrar
    if (text == NULL) {
        return;
    }

    // Normaliza el desplazamiento para que quede en el rango [0, 25]
    shift = shift % 26;
    if (shift < 0) {
        shift += 26;
    }

    // Recorre el texto y aplica el desplazamiento únicamente a letras
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        }
    }
}

int main(int argc, char *argv[]) {
    // Desplazamiento por defecto
    int shift = 3;

    // Si se pasa un argumento, se interpreta como desplazamiento.
    // Uso: ./programa [desplazamiento]
    if (argc == 2) {
        char *endptr = NULL;
        long parsed = strtol(argv[1], &endptr, 10);
        if (endptr == NULL || *endptr != '\0') {
            fprintf(stderr, "El desplazamiento debe ser un entero.\n");
            return 1;
        }
        shift = (int)parsed;
    } else if (argc > 2) {
        fprintf(stderr, "Uso: %s [desplazamiento]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 3\n", argv[0]);
        return 1;
    }

    // Buffer para leer la línea con fgets
    char buffer[MAX_LINE_LENGTH];

    // Solicita al usuario el texto a cifrar
    printf("Introduce el texto a cifrar: ");

    // Lee una línea desde la entrada estándar (stdin)
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        fprintf(stderr, "Error al leer la línea.\n");
        return 1;
    }

    // Elimina el salto de línea final que deja fgets (si lo hay)
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    // Cifra en sitio y muestra el resultado
    encryptCaesar(buffer, shift);
    printf("Texto cifrado: %s\n", buffer);

    return 0;
}


