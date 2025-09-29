// Problema 6 del Proyecto de EULER
// https://projecteuler.net/problem=6

#include <stdio.h>

int main(){

long suma_cuadr = 0, suma = 0; 
    
    for(int i = 1; i <= 100; i++){
        suma_cuadr += i*i; // suma de cuadrados
        suma += i;  // suma normal (luego se saca su cuadrado)
    }
    
    
    long cuadr_suma = suma * suma; // cuadrado de sumas
    
    long diferencia = cuadr_suma - suma_cuadr;
    
    printf("La diferencia es %ld\n", diferencia);
    
    return 0;
    
}

