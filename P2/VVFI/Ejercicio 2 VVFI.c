#include <stdio.h>

int main() {
    char buffer[100];  // Buffer para almacenar la cadena leída

    printf("Por favor, ingresa una línea de texto (máximo 99 caracteres):\n");

    // fgets lee una línea desde stdin (teclado), hasta 99 caracteres o hasta un salto de línea
    // fgets guarda el salto de línea '\n' si cabe en el buffer
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Aquí buffer contiene la cadena leída, incluyendo el salto de línea

        // Imprimimos el texto ingresado
        printf("Texto leído: %s", buffer);
    } else {
        // Si fgets falla (EOF o error)
        printf("Error al leer la entrada.\n");
    }

    return 0;
}
