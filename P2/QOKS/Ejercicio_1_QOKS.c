/*
 * Código de cifrado César.
 * 
 * Este programa cifra texto usando el cifrado César con un desplazamiento especificado.
 * 
 * Compilación:
 *   gcc Ejercicio_1_QOKS.c -o Ejercicio_1_QOKS
 * 
 * Uso:
 *   ./Ejercicio_1_QOKS "<texto_a_cifrar>" <desplazamiento>
 * 
 * Ejemplo:
 *   ./Ejercicio_1_QOKS "steve" 2
 *   Resultado: "uvgxg"
 * 
 * @author steve-quezada
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

// Función para cifrar texto usando cifrado César
// Referencia: server.c (función decryptCaesar)
// La diferencia principal es que sumamos el shift en lugar de restarlo
void Ejercicio_1_QOKS(char *text, int shift)
{
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        if (isupper(c))
        {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        }
        else if (islower(c))
        {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        else
        {
            text[i] = c;
        }
    }
}

// Función para convertir a minúsculas
// Referencia: server.c (función toLowerCase)
void toLowerCase(char *str)
{
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
// Referencia: server.c (función trim)
void trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}

int main(int argc, char *argv[])
{
    // Referencia: client.c Validación de argumentos de línea de comandos
    if (argc != 3)
    {
        printf("Usage: %s <text_to_encrypt> <shift>\n", argv[0]);
        printf("Example: %s \"hello world\" 3\n", argv[0]);
        return 1;
    }
    
    char *text = argv[1];           
    char *shift_str = argv[2];      
    int shift = atoi(shift_str);    
    
    char buffer[BUFFER_SIZE] = {0};
    
    // Copiar el texto a cifrar al buffer
    // Referencia: server.c Uso de strncpy para copiar strings de forma segura
    strncpy(buffer, text, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    // Formato de mensaje de texto original
    printf("[+] Text: %s\n", buffer);
    printf("[+] Shift: %d\n", shift);
    
    // Cifrar el texto usando la función Ejercicio_1_QOKS
    Ejercicio_1_QOKS(buffer, shift);
    
    // Formato de mensaje de texto cifrado
    printf("[+] Encrypted text: %s\n", buffer);
    
    return 0;
}