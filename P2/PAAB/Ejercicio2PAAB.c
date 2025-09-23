#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
// Uso de fgets
// Función para limpiar el salto de línea que deja fgets
void limpiarNuevaLinea(char *cadena) {
    size_t n = strlen(cadena);
    if (n > 0 && cadena[n - 1] == '\n') {
        cadena[n - 1] = '\0';
    }
}

int main(void) {
    char nombre[50];     // buffer para el nombre
    char edadTexto[10];  // buffer para leer la edad como texto
    int edad;

    // Lectura del nombre
    printf("Introduce tu nombre: ");
    if (fgets(nombre, sizeof(nombre), stdin) == NULL) {
        printf("Error al leer el nombre\n");
        return 1;
    }
    limpiarNuevaLinea(nombre); // quitar el salto de línea de fgets
    printf("Nombre capturado: %s\n", nombre);

    // Lectura de la edad
    printf("Introduce tu edad: ");
    if (fgets(edadTexto, sizeof(edadTexto), stdin) == NULL) {
        printf("Error al leer la edad\n");
        return 1;
    }
    limpiarNuevaLinea(edadTexto);

    // Convertir el texto a número
    edad = atoi(edadTexto);
    if (edad <= 0) {
        printf("Edad inválida: %s\n", edadTexto);
        return 1;
    }
    printf("Edad capturada: %d\n", edad);

    // Prueba
    printf("\nResumen de datos ingresados:\n");
    printf("Nombre: %s\n", nombre);
    printf("Edad: %d\n", edad);

    return 0;
}
