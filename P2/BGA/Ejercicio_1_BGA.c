// Ejercicio_1_BGA.c
// Compilar: gcc Ejercicio_1_BGA.c -o Ejercicio_1_BGA
// Ejecutar: ./Ejercicio_1_BGA "texto a cifrar" "desplazamiento"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static void cifradoCesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            text[i] = c;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s \"texto a cifrar\" <desplazamiento>\n", argv[0]);
        return 1;
    }
    int shift = atoi(argv[2]);
    // copiamos el texto porque argv puede ser const
    char buf[4096];
    strncpy(buf, argv[1], sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    cifradoCesar(buf, shift);
    printf("%s\n", buf);
    return 0;
}
