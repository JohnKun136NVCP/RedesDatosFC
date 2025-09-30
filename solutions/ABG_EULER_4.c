#include <stdio.h>
#include <stdbool.h>

static inline bool es_palindromo(int n) {
    int original = n, rev = 0;
    while (n > 0) {
        rev = rev * 10 + (n % 10);
        n /= 10;
    }
    return rev == original;
}

int main(void) {
    int mejor = 0, a_mejor = 0, b_mejor = 0;

    // Recorremos de 999 hacia 100 
    for (int a = 999; a >= 100; --a) {
        // Si aun con 999 ya no supera 'mejor', salimos
        if (a * 999 < mejor) break;

        for (int b = 999; b >= a; --b) {    
            int prod = a * b;

            if (prod <= mejor) break;        
            if (es_palindromo(prod)) {
                mejor = prod;
                a_mejor = a;
                b_mejor = b;
                // seguimos por si hay otro palíndromo mayor con este 'a'
            }
        }
    }

    printf("Mayor palindromo de dos numeros de 3 digitos: %d = %d x %d\n",
           mejor, a_mejor, b_mejor);
    return 0;
}
