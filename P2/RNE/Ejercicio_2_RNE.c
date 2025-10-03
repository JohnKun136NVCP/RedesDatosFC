#include <stdio.h>
#include <string.h>

// Definimos una constante para el tamaño máximo de cada línea
#define MAX_LINE_LENGTH 256

int main() {
    // Declaramos un puntero a un archivo (FILE). Este puntero "apuntará" a nuestro archivo una vez abierto.
    FILE *filePointer;

    // Declaramos un buffer para almacenar cada línea que leamos.
    char lineBuffer[MAX_LINE_LENGTH];

    // Nombre del archivo que vamos a leer.
    const char *filename = "poema.txt";

    // Abrimos el archivo en modo lectura ("r" de "read").
    // devuelve un puntero al archivo.
    // Si falla, devuelve NULL.
    filePointer = fopen(filename, "r");

    // Checamos si el archivo se pudo abrir.
    if (filePointer == NULL) {
        // imprimimos nuestro mensaje y también el error específico del sistema.
        perror("Error al abrir el archivo");
        return 1;
    }

    // Informamos al usuario que estamos a punto de leer el archivo.
    printf("Contenido del archivo '%s':\n", filename);
    printf("----------------------------------\n");

    /*
     * Llama a fgets() para intentar leer la siguiente línea del archivo.
     * Comprueba si el resultado de fgets() es diferente de NULL.
     * fgets() devuelve NULL cuando ya no hay más líneas que leer o si ocurre un error.
     */
    while (fgets(lineBuffer, MAX_LINE_LENGTH, filePointer) != NULL) {
        // printf() imprime la línea que acabamos de leer en la pantalla.
        printf("%s", lineBuffer);
    }
    printf("----------------------------------\n");

    // Cerrar el archivo.
    fclose(filePointer);

    return 0;
}
