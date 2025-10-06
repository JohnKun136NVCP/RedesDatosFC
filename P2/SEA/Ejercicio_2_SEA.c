#include <stdio.h>
#include <string.h>

int main() {
    char nombre[50];
    int edad;
    
    // fgets lee una línea completa incluyendo espacios
    // Sintaxis: fgets(variable, tamaño, origen)
    printf("Ingrese su nombre completo: ");
    fgets(nombre, sizeof(nombre), stdin);
    
    // Se elimina el salto de línea
    nombre[strcspn(nombre, "\n")] = '\0';
    
    printf("Ingrese su edad: ");
    scanf("%d", &edad);
    
    // Se limpia el buffer de entrada después de scanf
    while (getchar() != '\n');
    
    printf("\nDatos ingresados:\n");
    printf("Nombre: %s\n", nombre);
    printf("Edad: %d años\n", edad);
    
    return 0;
}