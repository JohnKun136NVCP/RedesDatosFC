#include <stdio.h>

/**
 * Calcula la función totient (phi) de Euler para un número n
 * 
 * La función phi(n) cuenta cuántos números entre 1 y n son primos relativos con n
 * (es decir, números cuyo máximo común divisor con n es 1)
 * 
 * @param n El número para calcular phi(n)
 * @return El valor de phi(n) - cantidad de números primos relativos con n
 */
int phi(int n) {
    int result = n; // Inicializamos con n, luego restaremos los no primos relativos
    
    // Factorizamos n en sus factores primos
    for (int p = 2; p * p <= n; p++) {
        // Si p es factor primo de n
        if (n % p == 0) {
            // Eliminamos todas las ocurrencias de p como factor
            while (n % p == 0) {
                n /= p;
            }
            // Restamos los múltiplos de p (no primos relativos)
            result -= result / p;
        }
    }
    
    // Si queda un factor primo mayor que 1 (n era primo)
    if (n > 1) {
        result -= result / n;
    }
    
    return result;
}

/**
 * Programa principal que encuentra el número n ≤ límite que maximiza n/phi(n)
 * 
 * Basado en el problema de Project Euler #69:
 * "Encuentra el valor de n ≤ 1,000,000 para el cual n/phi(n) es máximo"
 * 
 * El programa pide al usuario un límite máximo y busca la solución óptima
 */
int main() {
    int limite;
    
    printf("Encontrar n que maximiza n/phi(n)\n");
    printf("==================================\n");
    printf("Problema basado en Project Euler #69\n\n");
    
    // Solicitar el límite máximo al usuario
    printf("Ingresa el limite maximo: ");
    scanf("%d", &limite);
    
    // Validar que el límite sea válido
    if (limite < 2) {
        printf("Error: El limite debe ser mayor o igual a 2\n");
        return 1;
    }
    
    // Variables para trackear el máximo encontrado
    int max_n = 0;          // n que da el máximo ratio
    double max_ratio = 0.0; // Valor máximo de n/phi(n)
    
    printf("\nBuscando para n <= %d...\n", limite);
    
    // Probar todos los números desde 2 hasta el límite
    for (int n = 2; n <= limite; n++) {
        int totient = phi(n);           // Calcular phi(n)
        double ratio = (double)n / totient; // Calcular n/phi(n)
        
        // Si encontramos un nuevo máximo, actualizar
        if (ratio > max_ratio) {
            max_ratio = ratio;
            max_n = n;
        }
    }
    
    // Mostrar resultados
    printf("\n=== RESULTADO ===\n");
    printf("n que maximiza n/phi(n): %d\n", max_n);
    printf("phi(%d) = %d\n", max_n, phi(max_n));
    printf("%d / phi(%d) = %.6f\n", max_n, max_n, max_ratio);
    printf("\nExplicacion: n = %d produce el mayor valor de n/phi(n) ", max_n);
    printf("para todos los n <= %d\n", limite);
    
    return 0;
}