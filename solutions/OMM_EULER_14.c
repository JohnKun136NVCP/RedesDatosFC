#include <stdio.h>

int main() {
    const int limite = 1000000;

    long max_longitud = 0;      
    long num_inicial_max = 0;   

    for (long i = 1; i < limite; i++) {
        
        unsigned long long n = i;
        
        long longitud_actual = 1;

        while (n != 1) {
            if (n % 2 == 0) {
                n = n / 2;
            } else {
                n = 3 * n + 1;
            }
            longitud_actual++;
        }

        if (longitud_actual > max_longitud) {
            max_longitud = longitud_actual;
            num_inicial_max = i;
        }
    }

    printf("El número inicial que produce la cadena más larga es: %ld\n", num_inicial_max);
    printf("Su cadena cuenta con %ld términos.\n", max_longitud);

    return 0; 
}
