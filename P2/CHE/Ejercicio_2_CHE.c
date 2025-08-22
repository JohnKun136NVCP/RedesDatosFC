#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN 256 // Longitud máxima de la cadena.

// Función que lee la entrada del usuario.
void leer_entrada(char *buffer) {
    printf("Ingrese una palabra o frase: ");
    fgets(buffer, MAX_LEN, stdin); // Uso de fgets ;)
    
    // Remueve el salto de línea si existe.
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
}

// Función que limpia la cadena (solo letras, convierte a minúsculas).
void limpiar_cadena(char *original, char *limpia) {
    int j = 0;
    for (int i = 0; original[i] != '\0'; i++) {
        if (isalpha(original[i])) {
            limpia[j] = tolower(original[i]);
            j++;
        }
    }
    limpia[j] = '\0';
}

// Función que verifica si es palíndromo.
int es_palindromo(char *cadena) {
    int len = strlen(cadena);
    for (int i = 0; i < len / 2; i++) {
        if (cadena[i] != cadena[len - 1 - i]) {
            return 0; // No es palíndromo.
        }
    }
    return 1; // Sí es palíndromo.
}

int main() {
    char entrada[MAX_LEN];
    char cadena_limpia[MAX_LEN];

    leer_entrada(entrada);

    limpiar_cadena(entrada, cadena_limpia);
    
    if (es_palindromo(cadena_limpia)) {
        printf("Es un palíndromo.\n");
    } else {
        printf("No es un palíndromo.\n");
    }
    
    return 0;
}