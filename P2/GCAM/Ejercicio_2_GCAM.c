#include <stdio.h>
#include <string.h>

int main() {
    // Inicializamos las variables
    char nombre[50];
    char cuenta[10];
    
    // fgets() lee una línea completa incluyendo espacios
    // Sintaxis: fgets(var, espacio, fuente)
    // donde:
    // - var: variable donde se almacena el texto
    // - tamaño: máximo de caracteres a leer
    // - fuente: se utiliza stdin para una entrada estandar
    
    printf("Ingresa tu nombre: ");
    fgets(nombre, sizeof(nombre), stdin);
    
    // Eliminamos el salto de línea si existe
    nombre[strcspn(nombre, "\n")] = '\0';
    
    printf("Ingresa tu numero de cuenta: ");
    fgets(cuenta, sizeof(cuenta), stdin);
    cuenta[strcspn(cuenta, "\n")] = '\0';

    // Eliminamos el salto de línea si existe
    cuenta[strcspn(cuenta, "\n")] = '\0';
    
    
    // Mostrar los datos que se alamacenaron en fgets();
    printf("\n* * * Resultado: * * *\n");
    printf("Nombre: %s\n", nombre);
    printf("No. Cuenta: %s\n", cuenta);
    return 0;
}