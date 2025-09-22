#include <stdio.h>
#include <string.h>

// Función para la interacción del usuario a través del standard input.
// Esta función se mantiene en un bucle mientras el usuario no escriba la 
// cadena correcta, en este caso, el nombre del pokemon favorito de Anshar.
int main() {
    const char *pokemonFavorito = "Totodile";
    char intento[50];

    printf("Adivina mi Pokémon favorito (iniciando con mayuscula):\n");

    //bucle infinito
    while (1) {
        printf("Tu intento: ");
        
        // leemos del standard input con fgets
        if (fgets(intento, sizeof(intento), stdin) == NULL) {
            printf("Error leyendo entrada.\n"); // Evitamos errores para cadenas invalidas
            return 1;
        }

        // Eliminamos el salto de linea
        intento[strcspn(intento, "\n")] = '\0';

        // Corroboramos que el nombre del Pokemon sea el correcto
        if (strcmp(intento, pokemonFavorito) == 0) {
            printf("¡Correcto! Mi Pokémon favorito es %s\n", pokemonFavorito);
            break;
        } else {
            printf("No es %s. Intenta otra vez. HINT: Es el 158 de la Pokedex\n", intento);
        }
    }

    return 0;
}
