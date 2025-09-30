// Últimos 10 dígitos de la suma: 1^1 + 2^2 + ... + 1000^1000
#include <stdio.h>
#include <stdint.h>

typedef unsigned long long ull;
static const ull MOD = 10000000000ULL; // 10^10

// multiplicación modular segura (sin generar overflow):
#ifdef __SIZEOF_INT128__
static inline ull mul_mod(ull a, ull b) {
    return (ull)((__uint128_t)a * b % MOD);
}
#else
// fallback portable: suma-dobla
static inline ull mul_mod(ull a, ull b) {
    ull r = 0;
    while (b) {
        if (b & 1) { r += a; if (r >= MOD) r -= MOD; }
        a <<= 1; if (a >= MOD) a -= MOD;
        b >>= 1;
    }
    return r;
}
#endif

// exponenciación binaria: base^exp (mod MOD)
static ull pow_mod(ull base, ull exp) {
    ull res = 1 % MOD;
    base %= MOD;
    while (exp) {
        if (exp & 1) res = mul_mod(res, base);
        base = mul_mod(base, base);
        exp >>= 1;
    }
    return res;
}

int main(void) {
    ull suma = 0;
    for (ull i = 1; i <= 1000; ++i) {
        suma += pow_mod(i, i);
        if (suma >= MOD) suma -= MOD;
    }
    // imprime exactamente 10 dígitos (incluye ceros a la izquierda si hicieran falta)
    printf("Los últimos 10 dígitos de la suma: 1^1 + 2^2 + ... + 1000^1000 son... %010llu\n", suma);
    return 0;
}

