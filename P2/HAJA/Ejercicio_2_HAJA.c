// fgets
#include <stdio.h>
#include <string.h>

/* Mini demo de fgets():
   - Lee hasta tam-1 o hasta '\n'.
   - Mete '\0' al final si hay espacio (no deja la cadena “abierta”).
   - Si entra el '\n', hay que quitarlo a mano si molesta.
*/
int main(void) {
    char buffer[64]; // tamaño modesto, suficiente pa' un nombre

    printf("Escribe tu nombre completo y dale enter: ");

    if (!fgets(buffer, sizeof(buffer), stdin)) {
        fprintf(stderr, "Uy... no pude leer :(\n");
        return 1;
    }

    // si quedó el '\n' al final, lo volamos
    buffer[strcspn(buffer, "\n")] = '\0';

    printf("Hola, %s! Mucho gusto\n", buffer);
    return 0;
}
