# García Gómez Eduardo Biali
# 320113987
print("Numeros Pares Fibonacci")

limite = 4000000   
a = 1              
b = 2             
suma = 0           # variable para la suma

# genero Fibonacci y voy sumando si es par
while a <= limite:
    if (a & 1) == 0:   # bitwise: si el ultimo bit es 0, el numero es par
        suma += a
    a, b = b, a + b    # siguiente termino de la sucesion

print(suma)
