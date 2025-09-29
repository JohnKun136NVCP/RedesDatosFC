# -*- coding: utf-8 -*-
"""
Created on Mon Sep 29 12:25:24 2025

@author: Admin
"""

def suma_multiplos_3_5(limite):
    """
    Calcula la suma de todos los múltiplos de 3 o 5 menores que 'limite'.
    """
    suma = 0
    for n in range(limite):
        if n % 3 == 0 or n % 5 == 0:
            suma += n
    return suma

# Ejemplo: múltiplos menores a 10
print("Suma menores a 10:", suma_multiplos_3_5(10))  # Resultado esperado: 23

# Ejemplo: múltiplos menores a 1000
print("Suma menores a 1000:", suma_multiplos_3_5(1000))
