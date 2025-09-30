#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int **triangulo;
int lineas;
int **mejoresValores;

void contarLineas(char *ruta)
{
    // buffer de tamaño 512 para poder leer líneas completas
    char c;
    lineas = 0;
    FILE *archivo = fopen(ruta, "r");

    if (archivo == NULL)
    {
        printf("No se pudo abrir el archivo.\n");
        lineas = -1;
        return;
    }
    while ((c = fgetc(archivo)) != EOF)
    {
        if (c == '\n')
            lineas++;
    }
    fclose(archivo);
}

void imprimirTriangulo()
{
    for (int i = 0; i < lineas; i++)
    {
        for (int j = 0; j <= i; j++)
        { // cada fila tiene i+1 columnas
            printf("%d ", triangulo[i][j]);
        }
        printf("\n");
    }
}

int llenarTriangulo(char *ruta)
{
    char buffer[512];
    contarLineas(ruta);
    FILE *archivo = fopen(ruta, "r");

    if (archivo == NULL)
    {
        printf("No se pudo abrir el archivo.\n");
        return -1;
    }

    if (lineas == -1)
    {
        printf("Error al leer el archivo\n");
        return -1;
    }

    // Reservamos memoria para lineas*(tamaño de los punteros int)
    triangulo = (int **)malloc(lineas * sizeof(int *));
    mejoresValores = (int **)malloc(lineas * sizeof(int *));
    int elementos = 1;
    for (int i = 0; i < lineas; i++)
    {
        if (fgets(buffer, sizeof(buffer), archivo) != NULL)
        {
            int j = 0;
            // Reservamos memoria para cada nivel del triángulo
            triangulo[i] = (int *)malloc(elementos * sizeof(int));
            mejoresValores[i] = (int *)malloc(elementos * sizeof(int));
            elementos++;

            // Procesar la línea leída
            char *token = strtok(buffer, " ");

            // Para cada match lo intentamos convertir a entero
            while (token != NULL)
            {
                triangulo[i][j] = atoi(token);
                mejoresValores[i][j] = 0; // Inicializamos con ceros
                token = strtok(NULL, " ");
                j++;
            }
        }
    }
    return 0;
}

int caminoMax()
{
    // Empezamos desde la penúltima fila y vamos hacia arriba
    for (int i = lineas - 1; i >= 0; i--)
    {
        for (int j = 0; j <= i + 1; j++)
        {
            if (i == lineas - 1) // Si estamos en la última fila
            {
                mejoresValores[i][j] = triangulo[i][j];
            }
            else
            {
                // Tomar el valor máximo entre los dos hijos y sumar el valor propio
                int valorIzquierda = mejoresValores[i + 1][j];
                int valorDerecha = mejoresValores[i + 1][j + 1];
                if (valorIzquierda > valorDerecha)
                {
                    mejoresValores[i][j] = triangulo[i][j] + valorIzquierda;
                }
                else
                {
                    mejoresValores[i][j] = triangulo[i][j] + valorDerecha;
                }
            }
        }
    }
    // El valor máximo estará en la raíz del triángulo
    return mejoresValores[0][0];
}

int main(int argc, char *argv[])
{
    // leer ruta del archivo de terminal
    if (argc < 2)
    {
        printf("Uso: %s <ruta_del_archivo>\n", argv[0]);
        return 1;
    }
    char *ruta = argv[1];
    llenarTriangulo(ruta);
    // imprimirTriangulo();
    printf("El camino máximo tiene un valor de: %d\n", caminoMax());
    return 0;
}
