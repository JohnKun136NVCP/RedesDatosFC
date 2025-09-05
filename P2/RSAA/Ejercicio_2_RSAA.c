/*
 * Ejercicio 2: Encriptación César con entrada por fgets
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
#include <string.h>
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

int main() {
    // fgets nos exige un tamaño de búfer determinado. Vamos a ponerlo en 256 caracteres.
    char text[256];
    int shift;
    // Leemos el stream de entrada del usuario (stdin) con fgets.
    printf("Ingresa el mensaje para encriptar: ");
    fgets(text, sizeof(text), stdin);
    // Quitamos los caracteres de quiebre de línea (si es que los hay).
    text[strcspn(text, "\n")] = '\0';
    // Leemos otra vez el stream de entrada, ahora para obtener el desplazamiento
    // Como vamos a leer un solo segmento y este será de tipo entero, vamos a usar scanf.
    printf("Indica un número de desplazamiento: ");
    scanf("%d", &shift);
    // Ya con las entradas, encriptamos usando el código previo.
    encryptCaesar(text, shift);
    printf("Mensaje encriptado: %s\n", text);
    return 0;
}

