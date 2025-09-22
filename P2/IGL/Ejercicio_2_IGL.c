/* P2/IGL/Ejercicio 2 IGL.c
   Ejemplo de fgets(): lee una línea completa de forma segura y la muestra.
   Nota: fgets guarda el '\n' si la línea cupo en el buffer.
*/
#include <stdio.h>

int main(void) {
    char linea[128];
    printf("Escribe una línea: ");
    if (fgets(linea, sizeof(linea), stdin)) {
        printf("Leí: %s", linea);
    } else {
        puts("No se leyó nada (EOF o error).");
    }
    return 0;
}
