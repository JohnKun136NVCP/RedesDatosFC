/**
 * Se reolverá el ejercicio 344 de leetcode.
 * Cuya descripción es la siguiente:
 * "Write a function that reverses a string. 
 * The input string is given as an array of characters s.
 * You must do this by modifying the input array in-place with O(1) extra memory."
*/

/**
 * Se importa la biblioteca.
*/

#include <stdio.h>
#include <string.h>

// Programa que devuelve la reversa de una cadena
int main(){
    //Buffer para la palabra recibida
    char palabra[100];
    // Buffer para la palabra final
    char fin[] = "\n";
    // Bucle
    while(1 == 1){
        printf("Ingresa la palabra a voltear o enter para detener: ");
        // Uso de fgets
        if (fgets(palabra, sizeof(palabra),stdin) != NULL) {
            // Se compara la palabra con el enter para terminar la ejecución
            if (strcmp(palabra,fin) == 0)
                return 0;
            // ïndices para el intercambio de caracteres
            int i = 0;
            int j = strlen(palabra)-1;
            // Se intercambian los caracteres dado el indice
            while(i<j){
                char temp = palabra[i];
                palabra[i] = palabra[j];
                palabra[j] = temp;
                i++;
                j--;
            }
            printf("La palabra volteada es: %s\n",palabra);
        } else {
            printf("Error leyendo la palabra");
            return 1;
        }
    }
}