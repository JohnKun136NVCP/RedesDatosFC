#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TEXT_SIZE 100 // Tamaño maximo del arreglo

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) { 
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        } 
        else if (islower(c)) { 
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        } 
        else { text[i] = c;
        } 
    } 
}

int main() {
    char buffer[MAX_TEXT_SIZE];  // texto a encriptar
    char shiftStr[10];           // para leer el número como string
    int shift;                   // desplazamiento

    printf("Escribe el texto a encriptar (máximo 99 caracteres): "); // Solicitamos el texto al usuario
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) { // Utilizamos fgets para leer el texto
        printf("Escribe el desplazamiento (número entero): "); // Solicitamos el desplazamiento
        if (fgets(shiftStr, sizeof(shiftStr), stdin) != NULL) { // Utilizamos fgets para leer el desplazamiento
            shift = atoi(shiftStr);  // Convertimos el string a entero
            encryptCaesar(buffer, shift); // Aplicamos el cifrado
            printf("Texto encriptado: %s\n", buffer); // Mostramos el resultado
        } else {
            printf("Error al leer el desplazamiento.\n"); // En caso de fallo al leer mostramos un error
        }
    } else {
        printf("Error al leer el texto.\n"); // En caso de fallo al leer mostramos un error
    }

    return 0;
}