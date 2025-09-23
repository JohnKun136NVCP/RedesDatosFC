#include <stdio.h>

int main(void) {
    /* Definimos un buffer de 128 bytes para almacenar la línea leída. */
    char buffer[128];

    printf("Escribe una línea (máx. 127 caracteres): ");

    /* 
     * fgets() lee hasta N-1 caracteres de stdin (incluyendo los espacios)
     * o hasta encontrar un salto de línea '\n'. Siempre añade '\0' al final.
     * Si la línea introducida es más larga que el tamaño del buffer,
     * el resto de caracteres quedará pendiente en la entrada estándar.
     */
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        printf("La línea leída es: %s", buffer);
        /* Nota: fgets conserva el '\n' al final de la línea si cabe en el buffer. */
    } else {
        printf("No se pudo leer la línea.\n");
    }

    return 0;
}
