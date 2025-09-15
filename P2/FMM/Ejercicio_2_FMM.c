#include <stdio.h>
#include <string.h>

//Tamaño máximo del mensaje que puede ingresar el usuario
#define BUFFER_SIZE 1024

int main() {
    char pelicula[BUFFER_SIZE];
    
    printf("¿Cuál es tu película favorita?\n");

    // Leemos con fgets la película favorita del usuario
    if (fgets(pelicula, sizeof(pelicula), stdin) != NULL) {
        // fgets guarda el salto de línea, lo eliminamos
        size_t len = strlen(pelicula);
        if (len > 0 && pelicula[len - 1] == '\n') {
            pelicula[len - 1] = '\0';
        }

        printf("¡Wow! La película de %s es muy buena :)\n", pelicula);
    } else {
        printf("ERROR: No ingresaste ninguna película\n");
    }

    return 0;
}
