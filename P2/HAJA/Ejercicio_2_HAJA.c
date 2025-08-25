// fgets
#include <stdio.h>
#include <string.h>

/* fgets():
   - Lee hasta tam-1 o hasta '\n'.
*/
int main(void) {
    char buffer[64]; // tamaño modesto, suficiente para un nombre

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
