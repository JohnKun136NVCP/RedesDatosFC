// HAJA_EULER_4.c
// Joshua Abel Hurtado Aponte 

#include <stdio.h>
#include <stdbool.h>

static bool es_palindromo(int n) {
    int original = n;
    int rev = 0;
    while (n > 0) {
        rev = rev * 10 + (n % 10);
        n /= 10;
    }
    return rev == original;
}

int main(void) {
    int mejor = 0, a_mejor = 0, b_mejor = 0;

    for (int a = 999; a >= 100; --a) {
        int b_inicio = (a % 11 == 0) ? 999 : 990; 
        int paso = (a % 11 == 0) ? 1 : 11;

        for (int b = b_inicio; b >= 100; b -= paso) {
            int prod = a * b;
            if (prod <= mejor) break;

            if (es_palindromo(prod)) {
                mejor = prod;
                a_mejor = a;
                b_mejor = b;
                break;
            }
        }
        if (a * a <= mejor) break;
    }

    printf("Mayor palindromo = %d (%d x %d)\n", mejor, a_mejor, b_mejor);
    return 0;
}
//finn
