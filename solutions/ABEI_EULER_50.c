#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define LIMIT 1000000

// Verificar si un número es primo
bool isPrime(int n) {
    if (n < 2) return false;
    if (n % 2 == 0 && n != 2) return false;
    int root = (int)sqrt(n);
    for (int i = 3; i <= root; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Generar todos los primos menores a LIMIT
int generatePrimes(int *primes) {
    int count = 0;
    for (int i = 2; i < LIMIT; i++) {
        if (isPrime(i)) {
            primes[count++] = i;
        }
    }
    return count;
}

int main() {
    int *primes = malloc(sizeof(int) * LIMIT);
    int count = generatePrimes(primes);

    int maxLen = 0;
    int maxPrime = 0;

    // Probar todas las secuencias consecutivas
    for (int i = 0; i < count; i++) {
        int sum = 0;
        for (int j = i; j < count; j++) {
            sum += primes[j];
            if (sum >= LIMIT) break;

            if (isPrime(sum)) {
                int length = j - i + 1;
                if (length > maxLen) {
                    maxLen = length;
                    maxPrime = sum;
                }
            }
        }
    }

    printf("El primo menor a %d que puede escribirse como la suma de más primos consecutivos es %d\n", LIMIT, maxPrime);
    printf("Longitud de la secuencia: %d\n", maxLen);

    free(primes);
    return 0;
}
