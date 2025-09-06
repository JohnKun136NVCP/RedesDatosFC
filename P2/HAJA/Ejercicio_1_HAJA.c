// cifrado_cesar
// ejercicio 1
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/*  cifrar con César.
   - Solo letras (A-Z, a-z).
   - Espacios, números y signos no los toca
*/
void encryptCaesar(char *s, int shift) {
    shift %= 26; // por si pasas shifts gigantes
    for (int i = 0; s[i] != '\0'; i++) {
        unsigned char c = s[i];
        if (isupper(c))      s[i] = ((c - 'A' + shift) % 26) + 'A';
        else if (islower(c)) s[i] = ((c - 'a' + shift) % 26) + 'a';
        // si no es letra, lo dejamos 
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <shift>\n", argv[0]);
        return 1;
    }
    int shift = atoi(argv[1]);

    char buf[1024];
    // leo una línea completa 
    if (!fgets(buf, sizeof(buf), stdin)) {
        fprintf(stderr, "No se leyó nada :(\n");
        return 1;
    }
    // le quito el salto de línea si estorba
    buf[strcspn(buf, "\n")] = '\0';

    encryptCaesar(buf, shift);
    printf("%s\n", buf);
    return 0;
}
