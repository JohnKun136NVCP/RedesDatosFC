// par compilar: gcc "Ejercicio_2_BGA.c" -o "Ejercicio_2_BGA" 
// para correr:      ./Ejercicio_2_BGA   

#include <stdio.h>
#include <string.h>

int main(void) {
    char linea[128];

    printf("Escribe una cadena: ");
    // fgets lee hasta n-1 caracteres o hasta '\n' y SIEMPRE termina con '\0' si tuvo éxito
    if (fgets(linea, sizeof(linea), stdin) == NULL) {
        fprintf(stderr, "No se leyó entrada (EOF o error).\n");
        return 1;
    }

    // fgets guarda el salto de línea si cupo; lo quitamos para imprimir “limpio”
    linea[strcspn(linea, "\n")] = '\0';

    printf("Leído: \"%s\" (longitud=%zu)\n", linea, strlen(linea));
    return 0;
}
