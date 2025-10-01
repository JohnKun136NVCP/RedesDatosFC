#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Función recursiva para verificar si existe una partición válida
bool check_partition(const char *num, int index, long long current_sum, long long target) {
    if (index == strlen(num)) {
        return current_sum == target;
    }

    long long value = 0;
    for (int i = index; i < strlen(num); i++) {
        value = value * 10 + (num[i] - '0');
        if (check_partition(num, i + 1, current_sum + value, target)) {
            return true;
        }
    }
    return false;
}

// Verifica si un número n es un S-number
bool is_S_number(long long n) {
    long long root = sqrtl(n);
    if (root * root != n) return false;

    char buffer[32];
    sprintf(buffer, "%lld", n);

    // Necesitamos al menos 2 particiones
    int len = strlen(buffer);
    long long value = atoll(buffer);
    if (value == root) return false;

    return check_partition(buffer, 0, 0, root);
}

long long T(long long N) {
    long long sum = 0;
    for (long long k = 1; k * k <= N; k++) {
        long long n = k * k;
        if (is_S_number(n)) {
            sum += n;
        }
    }
    return sum;
}

int main() {
    // Prueba inicial: T(10^4) debe dar 41333
    printf("T(10^4) = %lld\n", T(10000));

    // Resultado final: T(10^12)
    printf("T(10^12) = %lld\n", T(1000000000000LL));

    return 0;
}
