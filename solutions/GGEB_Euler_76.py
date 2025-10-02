# García Gómez Eduardo Biali
# 320113987
print("Diferentes formas de sumar 100")

n = 100
# genero la lista de números con los que voy a sumar: 1..100
lista = []
for i in range(1, n + 1):
    lista.append(i)

print(lista)

# creo elarreglo donde voy contando cuantas formas hay de obtener cada total 0..100
formas = [0] * (n + 1)

formas[0] = 1

# recorro la lista por cada número i y voy actualizando las formas de construir los totales j
# uso j desde i para no contar permutaciones 
for i in lista:
    for j in range(i, n + 1):
        # sumo las nuevas maneras de llegar a j usando i
        formas[j] = formas[j] + formas[j - i]

respuesta = formas[n] - 1

print(f"El total de formas de sumar 100: {respuesta}")
