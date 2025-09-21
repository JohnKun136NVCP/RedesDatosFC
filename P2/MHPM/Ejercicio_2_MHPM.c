#include <stdio.h>
#include <stdlib.h>
 
/*
Código para usar fgets 
char *fgets( char *str, int count, FILE *stream );
Parámetros 
str: puntero a un elemento de una matriz char
count: núm máximo de caracteres que se pueden escribir
stream: flujo de archivos para leer los datos 

*/

#include <stdio.h>
#include <string.h>

int main(void)
{
    // PARÁMETRO str: Necesitamos un array de caracteres (buffer)
    char buffer[10];  // str: puntero a una matriz char - aquí buffer es el array
    
    printf("Escribe una cadena: ");
    
    // fgets( str, count, stream )
    char *resultado = fgets(
        buffer,      // str: puntero al array donde se guardará lo leído
        sizeof(buffer), // count: máximo de caracteres a escribir 
        stdin        // stream: flujo de entrada (teclado = stdin)
    );
    
    // Verificar si la lectura fue exitosa
    if (resultado != NULL) {
        // Eliminar el salto de línea si existe
        // (fgets incluye el \n en el buffer)
        if (buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';  // Reemplazar \n con \0
        }
        
        printf("\nContenido del buffer: \"%s\"\n", buffer);
        printf("Caracteres leídos: %zu\n", strlen(buffer));
    } else {
        printf("Error en la lectura\n");
    }
    
    return 0;
}
         

