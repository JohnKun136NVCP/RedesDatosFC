#include <stdio.h>

int main() {
    int monedas[] = {1, 2, 5, 10, 20, 50, 100, 200}; // denominaciones en peniques
    int cantidad_monedas = sizeof(monedas) / sizeof(monedas[0]);
    int objetivo = 200;

    long long formas[objetivo + 1]; // arreglo para guardar las combinaciones

    // inicializamos el arreglo con 0
    for (int i = 0; i <= objetivo; i++)
        formas[i] = 0;

    formas[0] = 1; // solo hay 1 forma de hacer 0 peniques

    // programación dinámica para contar las formas
    for (int i = 0; i < cantidad_monedas; i++) {
        for (int j = monedas[i]; j <= objetivo; j++) {
            formas[j] += formas[j - monedas[i]];
        }
    }

    printf("Número de formas de hacer £2: %lld\n", formas[objetivo]);
    return 0;
}

