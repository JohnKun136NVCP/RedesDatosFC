#include <stdio.h>
#include <string.h>

#define MAX 100  // tamaño del buffer 

int main(void) {
    char linea[MAX];

    printf("Escribe una línea (máx %d chars): ", MAX - 1);

    // fgets lee como máximo MAX-1 caracteres desde stdin,
    // se detiene en '\n' o EOF y SIEMPRE termina con '\0' si tuvo éxito.
    if (fgets(linea, sizeof linea, stdin) == NULL) { 
        puts("No se pudo leer entrada.");
        return 1;
    }
    // Elimina el '\n' final si quedó en la cadena.
    // strcspn busca la primera aparición de '\n' y devuelve su índice.
    linea[strcspn(linea, "\n")] = '\0';
    // Muestra lo leído
    printf("Se leyó: \"%s\"\n", linea);
    return 0;
}
