/*
 * Código de cifrado César con lectura de archivos usando fgets()
 * (Basado en Ejercicio_1_QOKS.c).
 * 
 * Compilación:
 *   gcc Ejercicio_2_QOKS.c -o Ejercicio_2_QOKS
 * 
 * Uso:
 *   ./Ejercicio_2_QOKS <archivo_entrada> <desplazamiento> <archivo_salida>
 * 
 * Ejemplo:
 *   ./Ejercicio_2_QOKS input.txt 2 output.txt
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
// Referencia: Ejercicio_1_QOKS.c (función Ejercicio_1_QOKS)
// Cambio de nombre de función para este ejercicio
void Ejercicio_2_QOKS(char *text, int shift)
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
    // Referencia: Ejercicio_1_QOKS.c - Validación de argumentos modificada para archivos
    if (argc != 4)
    {
        printf("Usage: %s <input_file> <shift> <output_file>\n", argv[0]);
        printf("Example: %s input.txt 3 output.txt\n", argv[0]);
        return 1;
    }
    
    char *input_file = argv[1];
    char *shift_str = argv[2];
    char *output_file = argv[3];
    int shift = atoi(shift_str);
    
    FILE *fp_input, *fp_output;
    char buffer[BUFFER_SIZE];
    
    // Referencia: server.c Apertura de archivos usando fopen
    fp_input = fopen(input_file, "r");
    if (fp_input == NULL)
    {
        printf("[-] Error opening input file: %s\n", input_file);
        return 1;
    }
    
    fp_output = fopen(output_file, "w");
    if (fp_output == NULL)
    {
        printf("[-] Error opening output file: %s\n", output_file);
        fclose(fp_input);
        return 1;
    }
    
    // Formato de mensajes informativos
    printf("[+] Input file: %s\n", input_file);
    printf("[+] Output file: %s\n", output_file);
    printf("[+] Shift: %d\n", shift);
    
    // Referencia: server.c (función saveNetworkInfo) Uso de fgets para leer línea por línea
    while (fgets(buffer, sizeof(buffer), fp_input) != NULL)
    {
        // Referencia: server.c (función isOnFile) Eliminación de salto de línea
        buffer[strcspn(buffer, "\n")] = '\0';
        
        // Formato de mensaje de texto original
        printf("[+] Text: %s\n", buffer);
        
        // Cifrar el texto usando la función Ejercicio_2_QOKS
        Ejercicio_2_QOKS(buffer, shift);
        
        // Formato de mensaje de texto cifrado
        printf("[+] Encrypted text: %s\n", buffer);
        
        // Referencia: server.c (función saveNetworkInfo) Escritura al archivo usando fprintf
        fprintf(fp_output, "%s\n", buffer);
    }
    
    // Cierre de archivos
    fclose(fp_input);
    fclose(fp_output);
    
    printf("[+] Encryption completed successfully!\n");
    
    return 0;
}
