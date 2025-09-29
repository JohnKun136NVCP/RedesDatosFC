#include <stdio.h>
#include <stdbool.h>

long long factorial(long long n) {
    if(n==0) return 1;
    return n * factorial(n-1);
}

long long permutacionLugar(long long lugar, long long tamano) {
    // No existe ese lugar
    if(factorial(tamano)<lugar || lugar < 0)
        return -1;
    // Valores iniciales
    long long acc = 1;
    long long permutacion = 0;
    bool ocupados[tamano];
    for (long long i = 0; i < tamano; i++) {
        ocupados[i] = false;
    }
    // Llenar cada posición
    for (long long i=tamano-1; i>=0; i--) {
        long long avance = factorial(i);
        // printf("Posición: %lld\n", i);
        long long numero = -1;
        while(acc <= lugar) {
            numero++;
            while(ocupados[numero]==true) numero++;
            // printf("numero %lld   avance %lld\n", numero, acc);
            acc += avance;
        }
        permutacion = permutacion * 10 + numero;
        // printf("permutacion %lld\n", permutacion);
        acc -= avance;
        ocupados[numero] = true;
    }
    return permutacion;
}

int main() {
    printf("%lld", permutacionLugar(1000000, 10));
    return(0);
}