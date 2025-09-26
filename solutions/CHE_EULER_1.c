#include <stdio.h>

// Función que calcula la suma de los múltiplos de un número dado por debajo de un número n.
// Progresión Aritmética.
int suma_multiplos(int k, int n) {
    int m = (n - 1) / k;
    return k * (m * (m + 1) / 2);
}

int main() {
    int n = 1000;
    // Suma de múltiplos de 3 + múltiplos de 5 - múltiplos de 15 (para evitar duplicados).
    printf("%d\n", suma_multiplos(3, n) + suma_multiplos(5, n) - suma_multiplos(15, n));
    // Resultado: 233,168.
}