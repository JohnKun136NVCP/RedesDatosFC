#include <stdio.h>
#include <math.h>//libreria para los calculos con logaritmos

// Funcion para calcular log(n!) usando la función gamma (calculo mas estable)
double log_comb(int n, int k) {
    return lgamma(n + 1) - lgamma(k + 1) - lgamma(n - k + 1);
}

int main() {
    int bolastotales = 70;          // Total de bolas
    int nummuestra = 20;          // Se seleccionan 20 bolas
    int numcolors = 7;            // 7 colores del arcoiris
    int bolasxcolor = 10;      // 10 bolas por color

    // Combinaciones: log(C(70, 20))
    double log_total = log_comb(bolastotales,nummuestra);
    
    // Combinaciones excluyendo un color: C(60, 20)
    double logsincol = log_comb(bolastotales - bolasxcolor,nummuestra);

    // Probabilidad de que un color ya no este
    double probcolaus = exp(logsincol - log_total);

    // Esperanza: 7 * (1 - probcolaus)
    double coloresesp = numcolors * (1.0 - probcolaus);

    printf("Numero de colores esperados: %.9f\n", coloresesp);

    return 0;
}