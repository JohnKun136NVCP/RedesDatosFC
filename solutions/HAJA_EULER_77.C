// HAJA_EULER_77.c
// Joshua Abel Hurtado Aponte 

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define LIM 300  // la n que buscamos es mucho antes

static int primos[LIM], pc = 0;
static void genera_primos(void) {
    bool esComp[LIM]; memset(esComp, 0, sizeof(esComp));
    for (int i = 2; i < LIM; ++i) if (!esComp[i]) {
        primos[pc++] = i;
        if ((long long)i * i < LIM)
            for (int j = i*i; j < LIM; j += i) esComp[j] = true;
    }
}

int main(void) {
    genera_primos();

    // ways[s] es el número de formas de escribir s como suma de primos 
    static int ways[LIM];
    ways[0] = 1; // base

    for (int i = 0; i < pc; ++i) {
        int p = primos[i];
        for (int s = p; s < LIM; ++s) ways[s] += ways[s - p];
    }

    // el primero con > 5000 formas
    for (int n = 2; n < LIM; ++n) {
        if (ways[n] > 5000) {
            printf("%d\n", n);
            return 0;
        }
    }
    return 0;
}
//finnn