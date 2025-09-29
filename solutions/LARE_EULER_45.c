#include <stdio.h>
#include <math.h>

/** 
 * ------ AVISO ------
 * Estamos usando la biblioteca math.h para resolver este ejercicio, especificamente se usa la 
 * funcion 'sqrt'. Por lo que se debe considerar que al compilar se debe especificar la biblioteca.
 * 
 * gcc LARE_EULER_45 -o <nombre_ejecutable> -lm
 * 
 * --------------------
*/


/** 
 * Verificamos si un numero es pentagonal.
 * Se sigue de la funcion n = sqrt(1 + 24*num)/6 + 1/6
 * Si n es un entero entero, entonces num es pentagonal, si es fraccional, no lo es.
 * @param num Numero a verificar
 * @return 1 si es pentagonal, 0 en otro caso
 */
int es_pentagonal(long long num) {
  double n = (1 + sqrt(1 + 24 * num)) / 6;
  return n == (long long)n;
}

/** 
 * Función principal
 */
int main() {
  // No es necesario saber el numero triangular, pues todo numero hexagonal es un numero triangular.
  // Si H(n) es hexagonal, entonces T(2n-1) es triangular y ademas H(n) = T(2n-1).
  long long hexagonal = 0;

  int encontrado = 0;
  // Empezamos en 144 puesto que 143 es el primer valor de n tal que T(n) = P(n) = H(n) = 40755
  int n = 144;
  while (!encontrado) {
    // Obtenemos el hexagonal
    hexagonal = n * (2 * n - 1);

    // Verificamos si es pentagonal
    if (es_pentagonal(hexagonal)) {
      encontrado = 1;
    }
    n++;
  }

  printf("El siguiente numero es triangular, pentagonal y hexagonal: %lld\n", hexagonal);
  return 0;
}
