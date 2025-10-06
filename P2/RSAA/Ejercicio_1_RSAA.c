/*
 * Ejercicio 1: Encriptación César
 * Elaborado por Alejandro Axel Rodríguez Sánchez
 * ahexo@ciencias.unam.mx
 *
 * Facultad de Ciencias UNAM
 * Redes de computadoras
 * Grupo 7006
 * Semestre 2026-1
 * 2025-08-23
 *
 * Profesor:
 * Luis Enrique Serrano Gutiérrez
 * Ayudante de laboratorio:
 * Juan Angeles Hernández
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
        // En la función "decryptCaesar" proporcionada, el módulo se sacaba dos veces y el desplazamiento
        // se hacía hacia la izquierda con una substracción del shift:
        // text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        // Para encriptar, tenemos que hacer el desplazamiento a la derecha (sumar el shift) y quitar un
        // módulo:
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main(int argc, char *argv[]) {
    // Para usar el programa, hay que pasarle la cadena y el desplazamiento (shift) como argumentos.
    // Ejemplo: ./ejercicio1 "HatsuneMiku" 39
    // Resultado: UngfharZvxh
    // (asumiendo que el programa se compiló con el nombre "ejercicio1")
    if (argc != 3) {
        printf("%s \"<text>\" <shift>\n", argv[0]);
        return 1;
    }
    char *text = argv[1];
    // Casteamos el shift en un entero (la entrada es un string)
    int shift = atoi(argv[2]);
    encryptCaesar(text, shift);
    printf("Cadena encriptada: %s\n", text); // Output the encrypted string
    return 0;
}
