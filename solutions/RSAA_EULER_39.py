"""
PROBLEMA #39
https://projecteuler.net/problem=39

Si $p$ es el perímetro de una triángulo recto cuyos lados son los enteros ${a, b, c}$
entonces hay exactamente tres soluciones para $p=120$:

$ {20, 48, 52}, {24, 45, 51}, {30, 40, 50} $

Entonces ¿para qué valor de p<=1000 se maximiza este número de soluciones?.
"""

import sys
import math

def gcd(a: int , b: int) -> int:
    """
    Máximo Común Divisor, lo ocupamos para verificar que m y n sean co-primos.

    Args:
        a (int): Entero arbitrario.
        b (int): Entero arbitrario.

    Returns:
        int: MCD
    """
    while b:
        a, b = b, a % b
    return a

def find_max_solutions_perimeter(p_limit: int) -> int:
    """
    Determina el perímetro p <= p_limit con el máximo número de soluciones de
    triángulos rectángulos de lados enteros utilizando la fórmula de Euclides.

    Args:
        p_limit (int): El perímetro máximo a comprobar.

    Returns:
        int: El valor del perímetro con más soluciones.
    """

    # Vamos a usar una lista para guardar el conteo de soluciones para cada perímetro.
    # El índice de cada entrada de la lista es su valor de p correspondiente.
    counts = [0] * (p_limit + 1)

    # La fórmula de Euclides para las tercias pitagóricas primitivas (a, b, c) es:
    # a = m^2 - n^2, b = 2mn, c = m^2 + n^2
    # El perímetro p = a + b + c = 2m(m + n).

    # Necesitamos hallar el límite de m.
    # Como n >= 1, el perímetro más pequeño es p = 2m(m+1).
    # Por lo tanto, 2m(m+1) <= p_limit => m^2 < m(m+1) <= p_limit/2.
    # Esto significa que m < sqrt(p_limit/2). Sumamos 1 para el límite del rango.
    m_limit = int(math.sqrt(p_limit / 2)) + 1

    for m in range(2, m_limit):
        for n in range(1, m):
            # Para los primitivos, se debe cumplir que:
            # 1. m > n
            # 2. m and n sean co-primos (Su GCD es 1).
            # 3. Alguno es par.
            if (m - n) % 2 == 1 and gcd(m, n) == 1:
                # Calculamos el perímetro de nuestra tercia primitiva.
                p = 2 * m * (m + n)
                k = 1
                while k * p <= p_limit:
                    counts[k * p] += 1
                    k += 1

    # Podemos encontrar el índice del valor máximo en nuestra lista de conteos.
    # El valor en un índice es el número de soluciones, y el índice en sí es el perímetro p.
    max_solutions = 0
    best_p = 0
    for i in range(p_limit + 1):
        if counts[i] > max_solutions:
            max_solutions = counts[i]
            best_p = i
            
    return best_p

def main():
    # Vamos a recibir un único argumento, el cual se trata de un entero.
    if len(sys.argv) != 2:
        print(f"Ejecutar tal que: python {sys.argv[0]} <p_limit: int>")
        sys.exit(1)
    try:
        p_limit = int(sys.argv[1])
        if p_limit < 12:
             print(f"Para p <= {p_limit}, no hay triángulos rectos de lados enteros.")
             print("Lo menos que se puede es uno con p=12 ({3, 4, 5}).")
             return
    except ValueError:
        print(f"Error: Tiene que ser un entero positivo, no como '{sys.argv[1]}'.")
        sys.exit(1)

    # Resultado
    result_p = find_max_solutions_perimeter(p_limit)
    print(f"Para {p_limit}, el valor de p que maximiza las soluciones posibles es: {result_p}, con {counts[result_p]}")

if __name__ == "__main__":
    main()

