// Euler 23: suma de enteros que NO son suma de dos abundantes

#include <stdio.h>

#define LIM 28123

// Suma de divisores propios (todos los divisores < n)
int suma_divisores_propios(int n) {
    if (n <= 1) return 0;
    int s = 1; // 1 siempre divide a n (si n>1)
    for (int d = 2; d <= n/2; d++) { 
        if (n % d == 0) s += d;
    }
    return s;
}

int main(void) {
    int abund[LIM + 1]; // guardará los abundantes encontrados
    int na = 0; // número de abundantes
    int es_suma[LIM + 1] = {0}; // 1 si puede escribirse como suma de dos abundantes

    // 1) encontrar números abundantes
    for (int n = 1; n <= LIM; n++) {
        int s = suma_divisores_propios(n);
        if (s > n) abund[na++] = n;
    }

    // 2) marcar todos los que sí son suma de dos abundantes
    for (int i = 0; i < na; i++) {
        for (int j = i; j < na; j++) { // no repetir pares
            int suma = abund[i] + abund[j];
            if (suma <= LIM) es_suma[suma] = 1;
            else break;
        }
    }

    // 3) sumar los que NO son suma de dos abundantes
    long long total = 0;
    for (int n = 1; n <= LIM; n++)
        if (!es_suma[n]) total += n;

    printf("%lld\n", total);
    return 0;
}
