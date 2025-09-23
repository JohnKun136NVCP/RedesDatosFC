#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    //Programa que le pide al usuario un numero de 5 digitos y utilizando fgets(), regresa solo los primeros 3
    char buffer[10];   // espacio para el número + '\n' + '\0'
    char primerosTres[4]; // 3 dígitos + '\0'

    printf("Ingresa un número de 5 dígitos: ");
    fgets(buffer, sizeof(buffer), stdin);

    // Copiamos solo los primeros 3 caracteres
    strncpy(primerosTres, buffer, 3);
    primerosTres[3] = '\0';
    printf("Los primeros 3 dígitos son: %s\n", primerosTres);

}
