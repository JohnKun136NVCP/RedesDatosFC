//Problema 3 de la página Project Euler 
#include <stdio.h>

// Función para encontrar el mayor factor primo de un número
long long mayor_factor_primo(long long n) {
    long long factor = 2;

    // Mientras el cuadrado del factor sea menor o igual al número
    while (factor * factor <= n) {
        // Si el número es divisible entre el factor actual
        if (n % factor == 0) {
            n /= factor;  // Dividimos el número entre el factor
        } else {
            factor++;     // Si no, probamos con el siguiente número
        }
    }

    // Al final, n será el mayor factor primo encontrado
    return n;
}

int main() {
    // Número dado por el problema
    long long numero = 600851475143;

    // Llamamos a la función para calcular el mayor factor primo
    long long resultado = mayor_factor_primo(numero);

    // Imprimimos el resultado
    printf("El mayor factor primo es: %lld\n", resultado);

    return 0;
}
