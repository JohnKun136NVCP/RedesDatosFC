/**
 * Implementaremos un código para encontrar la solución al 
 * problema 686 de Project Euler
 */

 /**
  * Tenemos p(L, n) donde 
  * L = dígitos iniciales 
  * n = exponente donde se encuentran esos dígitos
  * 
  * Como el problema que tenemos nos pide p(1233, 678910) el cual es
  * un núm muy grande vamos a usar logaritmos 
  */ 


#include <stdio.h>
#include <math.h>
#include <inttypes.h>

int main(void) {
    const long double log2_10 = log10l(2.0L);
    const long double low  = log10l(1.23L);  // límite inferior (decimal)
    const long double high = log10l(1.24L);  // límite superior (decimal)
    const int64_t TARGET = 678910; // el exponente que queremos encontrar 

    // primer exponente que empieza con 123, esto lo ponemos para optimizar el código, porque de lo contrario
    // se calcula exponente por exponente lo que causa que el programa tarde demasiado en ejecutar 
    int64_t i = 90;      
    int64_t rank = 0;
    int64_t last = 0;
    const long double eps = 1e-18L; // tolerancia para evitar problemas de redondeo

    // En esta parte ponemos intervalos, ya que el valor que vamos a calcular es muy grande.
    // Esto nos sirve para que a la hora de compilar el número que nos da el programa no haga uno 
    // por uno los cálculos si no que de saltos para que el tiempo de ejecución sea 
    // menor
    while (rank < TARGET) {
        long double x = i * log2_10;
        long double frac = x - floorl(x);  // parte fraccionaria en [0,1)
        if (frac + eps >= high) {
            // estamos por encima del intervalo -> dar salto pequeño
            i += 93;
        } else if (frac < low - eps) {
            // por debajo del intervalo -> dar salto grande
            i += 196;
        } else {
            // dentro del intervalo: encontramos un j válido
            last = i;
            rank++;
            i += 196; // avanzamos 196 tras encontrar uno, esto lo podemos saber 
            // por j * log_(10) (2)
        }
    }

    printf("p(123, %lld) = %" PRId64 "\n", (long long)TARGET, last);
    return 0;
}
