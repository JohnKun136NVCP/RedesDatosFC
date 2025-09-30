// Euler56 — Máxima suma de dígitos de a^b para a, b<100
#include <stdio.h>

// Se define una constante para el número máximo de dígitos que puede tener nuestro número.
// Lo dejo en 300 por el no vaya a ser
#define MAX_DIG 300   

// ---- Función que modifica un arreglo de enteros -----
// Voy a "resetear" el numero guardado en el arreglo para que valga 1, y servirá como
// mi punto de partida para empezar a calcular una potencia.
static void set_one(int d[], int *len) {
    // El primer digito, el de las unidades, es 1.
    d[0] = 1; 
    // La longitud actual del numero es 1.
    *len = 1;
    // Se limpia el resto del arreglo, llenándolo de ceros para evitar
    // que queden restos de cálculos anteriores.
    for (int i = 1; i < MAX_DIG; ++i) d[i] = 0;
}

// ----- Función para multiplicar el número grande almacenado en un arreglo d por un numero
// pequeño k. Esto lo hacemos con la metodología de la primaria -------
static void mul_small(int d[], int *len, int k) {
    // El acarreo que llevamos.
    int carry = 0;
    
    // Se recorre cada dígito del numero actual.
    for (int i = 0; i < *len; ++i) {
         // Se multiplica el dígito actual por k y se le suma el acarreo anterior.
        int x = d[i] * k + carry;
         // El nuevo dígito es el resto de la división entre 10.
        d[i] = x % 10;
        // El nuevo acarreo es el resultado de la división entera.
        carry = x / 10;
    }
    
    // Si aún queda acarreo, el número ha crecido.
    while (carry) {
        // Se añaden los dígitos restantes del acarreo al final del número.
        d[(*len)++] = carry % 10;
        carry /= 10;
    }
}

// ----- Función para calcular la suma de los valores en un arrgleo --------
static int digit_sum(const int d[], int len) {
    int s = 0;
    for (int i = 0; i < len; ++i) 
      s += d[i];
    return s;
}


// ------ MAIN -------
int main(void) {

    // Variable para guardar la suma de dígitos más alta encontrada hasta el momento.
    int max_sum = 0;
    // Arreglo donde haremos los cálculos.
    int d[MAX_DIG], len;

    // Iteramos a traves del numero base a desde 1 hasta 99.
    for (int a = 1; a < 100; ++a) {
        // Si a es multiplo de 10, nos lo brincamos. 
        if (a % 10 == 0) continue;
        // Iteramos a traves del exponente b desde 1 hasta 99.
        for (int b = 1; b < 100; ++b) {
            // Iniciamos el número en 1.
            set_one(d, &len);
            // Calcular a^b, multiplicando b veces por a.
            for (int i = 0; i < b; ++i) 
              mul_small(d, &len, a); 
            // Calculamos la suma de los digitos del resultado.
            int s = digit_sum(d, len);
            // Si la suma actual es mayor que la maxima registrada, se actualiza.
            if (s > max_sum) max_sum = s;
        }
    }
    
    // Vemos si nos salio, o no.
    printf("%d\n", max_sum);  
    return 0;
}
