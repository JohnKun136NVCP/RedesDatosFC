#include <stdio.h>
#include <stdbool.h>
#include <math.h>

//gcc CTAR_EULER10.c -o CTAR_EULER10 -lm

/*
    Funcion que calcula la sumatoria de los numeros primos menores a n
*/
long long sumaPrimosDebajo(int n) {
    int primosAprox;
    //Teorema de los numeros primos mas un margen de seguridad
    primosAprox = (int)(n / log(n) * 1.2); 
    int primos[primosAprox]; 
    int cantidadPrimos = 0;
    long long suma = 0; 

    for(int i = 2; i < n; i++) {
        bool esPrimo = true; 
        int raiz = (int)sqrt(i);
        //Verificamos si i es divisible por algun primo ya encontrado
        for(int j = 0; j < cantidadPrimos && primos[j] <= raiz; j++) {
            if(i % primos[j] == 0) {
                esPrimo = false;
                break;
            }
        }

        if(esPrimo) {
            primos[cantidadPrimos++] = i;
            suma += i;
        }
    }
    return suma;
}

/*
    Funcion principal donde probamos la funcion sumaPrimosDebajo de 2M
*/
int main() {
    int n = 2000000;
    long long resultado = sumaPrimosDebajo(n);
    printf("La sumatoria de los numeros primos menores a %d es: %lld\n", n, resultado);
    return 0;
}
