#include <stdio.h>

long long mcd(long long a, long long b) {
  if (b == 0) return a;
  return mcd(b, a % b);
}

long long mcm(long long a, long long b) {
  return ((a * b) / mcd(a,b));
}

int main() {
  long long numero = 1;
  for (long long i = 1; i <= 20; i++) {
    numero = mcm(numero, i);
  }

  printf("El numero es: %lld\n", numero);
  return 0;
}
