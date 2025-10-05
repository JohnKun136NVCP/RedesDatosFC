#include <stdio.h>

int main(void) {
    // factoriales de los dígitos del 0 al 9
    int fact[10];
    fact[0] = 1;
    for (int i = 1; i <= 9; ++i)
        fact[i] = fact[i - 1] * i;  // calcula cada factorial 


    // El número más grande posible está limitado por 7 * 9! = 2540160
    const int LIMIT = 7 * fact[9];
    long long total = 0;  // suma de los números que cumplen la condición

    // Recorremos todos los números desde 10 hasta el límite
    for (int n = 10; n <= LIMIT; ++n) {
        int x = n;
        int sum = 0;

        // Calcula la suma de los factoriales de los dígitos del número actual
        while (x > 0) {
            sum += fact[x % 10];  // añade el factorial del último dígito
            x /= 10;              // elimina el último dígito
        }

        // Si se cumple se añade al total
        if (sum == n)
            total += n;
    }

    printf("%lld\n", total);
    return 0;
}
