// Crea un programa que lleve la funci ́on fgets() y comenta el c ́odigo.

#include <stdio.h>

int main() {
    char buffer[100];

    // Usamos fgets para leer una línea de texto
    printf("Introduce un texto: ");
    fgets(buffer, sizeof(buffer), stdin);

    // Mostramos el texto introducido
    printf("Has introducido: %s\n", buffer);

    return 0;
}