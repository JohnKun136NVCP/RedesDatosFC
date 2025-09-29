#include <stdio.h>
#include <math.h>
#include <stdbool.h>

int esPrimo(int n){
    if (n == 1 || n == 0)
        return false;

    for (int i = 2; i < n; i++) {
        if (n % i == 0)
            return false;
    }
    return true;
}


int main(){
    int numeros[10001] = {0};
    int n = 1;
    int index_array = 0;
    while(numeros[10000] == 0){
        if (esPrimo(n)){
            numeros[index_array] = n;
            index_array++;
        }
        n++;
    }
    printf("El 10,001 numero primo es %d\n", numeros[10000]);
    return 0;
}