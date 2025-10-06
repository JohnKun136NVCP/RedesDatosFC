#include <stdio.h>
#include <string.h>

int main(void) {
    char buffer[64];

    printf("Dime tu instagram user: ");
    /* fgets lee hasta 63 chars o hasta '\n' y SIEMPRE coloca '\0' al final si hay espacio */
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        fprintf(stderr, "Error leyendo desde stdin.\n");
        return 1;
    }

    /* Eliminar el '\n' final si quedó almacenado */
    size_t n = strlen(buffer);
    if (n > 0 && buffer[n-1] == '\n') {
        buffer[n-1] = '\0';
    }

    printf("\nHola c: %s\n", buffer);
    return 0;
}
