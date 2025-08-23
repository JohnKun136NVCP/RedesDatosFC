#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Descifrado César.
void decryptCaesar(char *text, int shift) {
  shift = shift % 26;
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
    if (isupper(c)) {
      text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
    } else if (islower(c)) {
      text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
    } else {
      text[i] = c;
    }
  }
}

// Cifrado César.
void encryptCaesar(char *text, int shift) {
  shift = shift % 26;
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
    if (isupper(c)) {
      text[i] = ((c - 'A' + shift) % 26) + 'A';
    } else if (islower(c)) {
      text[i] = ((c - 'a' + shift) % 26) + 'a';
    } else {
      text[i] = c;
    }
  }
}

int main() {
  char texto[1024];   // Buffer para almacenar el texto de entrada.
  int desplazamiento; // Número de posiciones a desplazar en el alfabeto.
  int opcion;         // Opción del usuario (1=cifrar, 2=descifrar).

  // Muestra el menú y obtiene la opción del usuario.
  printf("Código de César - [1] Cifrar [2] Descifrar: ");
  scanf("%d", &opcion);
  getchar(); // Limpia el buffer de entrada

  // Obtiene el texto a procesar
  printf("Texto: ");
  fgets(texto, sizeof(texto), stdin);

  // Elimina el salto de línea al final del texto si existe
  if (texto[strlen(texto) - 1] == '\n') {
    texto[strlen(texto) - 1] = '\0';
  }

  // Obtiene el valor de desplazamiento
  printf("Desplazamiento: ");
  scanf("%d", &desplazamiento);

  // Procesa el texto según la opción seleccionada
  if (opcion == 1) {
    encryptCaesar(texto, desplazamiento);
    printf("Resultado: %s\n", texto);
  } else if (opcion == 2) {
    decryptCaesar(texto, desplazamiento);
    printf("Resultado: %s\n", texto);
  } else {
    printf("Opción inválida.\n");
    return 1;
  }

  return 0;
}
