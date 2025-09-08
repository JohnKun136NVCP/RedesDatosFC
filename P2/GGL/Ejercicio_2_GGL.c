#include <string.h>

int main() {
    char buffer[100];
    printf("Escribe algo: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';
    printf("You wrote: %s\n", buffer);
    printf("Longitud: %zu caracteres\n", strlen(buffer));
    return 0;
}