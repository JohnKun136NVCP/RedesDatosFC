// MMVM_EULER_145.c
#include <stdio.h>
#include <stdbool.h>
#include <string.h>  
#include <stdlib.h> 


// Las dos funciones auxiliares se que no siguen la estrategia más optima, 
// pero a problemas desesperados...

// ----- Función para obtener la reversa de un numero ------
// Por ejemplo, reverse(321) = 123.
long reverse(long n) {
    // Arreglo de char para guardar n.
    char num[30];
    // Arreglo de char para guardar la reversa de n.
    char rev[30];
    
    // Convertir de long a String.
    sprintf(num, "%ld", n);
    // longitud de num.
    int len = strlen(num);

    // for para cambiar el revertir el orden de num.
    for (int i = 0; i < len; i++) {
        rev[i] = num[len - 1 - i];
    }
    
    // Marca el fin.
    rev[len] = '\0';

    // Convertir de String a long.
    long resultado = atol(rev);
    
    // Regresar el resultado final.
    return resultado;
}


// ----- Función para verificar si todos los dígitos en un número son impares ---
bool digitosImpares(long n) {

    // Caso base.
    if (n == 0) 
      return false;
      
    // Arreglo de char para guardar n.
    char num[30];
    // Convertir de long a String.
    sprintf(num, "%ld", n);
    // longitud de num.
    int len = strlen(num);
    // Variables para guardar un digito.
    char dig;
    long digito;
    
    for (int i = 0; i < len; i++) {
    
        dig = (char)(num[i] + '0');
        digito = (long)(dig - '0');
        // Si un dígito es par, ya fue.
        if (digito % 2 == 0) {
            return false;  
        }
    }
    // Si llegamos hasta aquí, todo bien.
    return true;
}

// ------ MAIN --------
int main() {
    // Limite de la búsqueda.
    const long LIMIT = 100;
    // Contador de reversibles.
    int contador = 0;

    // for para ver todos los números y sus reverse.
    for (long n = 1; n < LIMIT; n++) {

        // Si el número termina en 0, su reverso empieza con 0 y no nos sirve.
        // Nos brincamos este caso.
        if (n % 10 == 0) {
            continue;
        }

        // Calculamos la suma de n y su reverso.
        long sum = n + reverse(n);

        // Verificamos si todos los dígitos de la suma son impares.
        if (digitosImpares(sum)) {
            contador++; // Si lo son, incrementamos nuestro contador.
        }
        
    }

    printf("%d\n", contador);

    return 0; // El programa termina.
}
