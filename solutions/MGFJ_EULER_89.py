

# Diccionarios de representación de numeros robamos
numeros_base = {'I': 1, 'V': 5, 'X': 10, 'L': 50, 'C': 100, 'D': 500, 'M': 1000}
numeros_resta = {'IV': 4, 'IX': 9, 'XL': 40, 'XC': 90, 'CD': 400, 'CM': 900}

def convertir_a_decimal(numero: str) -> tuple[int, int]:
    """
    Convierte un número romano a su valor decimal.
    
    Args:
        numero (str): Cadena que representa un número romano
    
    Returns:
        tuple[int, int]: Una tupla con (valor decimal, longitud original del número romano).
    """
    longitud = len(numero)
    numero_decimal = 0
    i = 0
    
    while i < longitud:
        if i + 1 < longitud and numero[i:i+2] in numeros_resta:
            numero_decimal += numeros_resta[numero[i:i+2]]
            i += 2
        elif numero[i] in numeros_base:
            numero_decimal += numeros_base[numero[i]]
            i += 1
        else:
            raise ValueError(f"Caracter no soportado en números romanos: {numero[i]}")
            
    return numero_decimal, longitud


def convertir_a_romano(numero: int) -> tuple[str, int]:
    """
    Convierte un valor decimal a su representación romana mínima.
    
    Args:
        numero (int): Valor decimal a convertir.
    
    Returns:
        tuple[str, int]: Una tupla con (cadena del número romano mínimo, su longitud).
    """
    caracteres = []
    numeros_romanos = (numeros_base | numeros_resta)
    for valor, caracter in sorted([(v, k) for k, v in numeros_romanos.items()], reverse=True):
        while numero >= valor:
            caracteres.append(caracter)
            numero -= valor

    numero_romano = "".join(caracteres)

    return numero_romano, len(numero_romano)



def leer_archivo(nombre_archivo: str) -> list[str]:
    """
    Lee un archivo de texto y devuelve una lista de líneas con números romanos.
    
    Args:
        nombre_archivo (str): Ruta del archivo a leer.
    
    Returns:
        list[str]: Lista de cadenas, cada una representando un número romano.
    """
    with open(nombre_archivo, 'r', encoding='utf-8') as archivo:
        lineas = archivo.readlines()
    return [linea.strip() for linea in lineas if linea.strip()]




if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print("Uso: python3 MGFJ_EULER_89.py <nombre_archivo>")
        sys.exit(1)

    nombre_archivo = sys.argv[1]
    lineas = leer_archivo(nombre_archivo)

    total_ahorro = 0

    for linea in lineas:
        decimal, longitud_original = convertir_a_decimal(linea)
        romano_min, longitud_min = convertir_a_romano(decimal)
        ahorro = longitud_original - longitud_min
        total_ahorro += ahorro

    print(f"Ahorro total de caracteres: {total_ahorro}")