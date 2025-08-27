#include <stdio.h>

int main() {
    char hero_favorite[100]; // Héroe favorito
    char map_favorite[100];  // Mapa favorito
    char experience_rating[100]; // Calificación de la experiencia

    printf("Encuesta de experiencia en Overwatch\n");

    printf("¿Cuál es tu héroe favorito de Overwatch? ");
    fgets(hero_favorite, sizeof(hero_favorite), stdin);

    printf("¿Cuál es tu mapa favorito de Overwatch? ");
    fgets(map_favorite, sizeof(map_favorite), stdin);

    printf("¿Cómo calificarías tu experiencia general en Overwatch del 1 al 10? ");
    fgets(experience_rating, sizeof(experience_rating), stdin);


    printf("\nGracias por completar la encuesta. Aquí están tus respuestas:\n");
    printf("Héroe favorito: %s", hero_favorite);
    printf("Mapa favorito: %s", map_favorite);
    printf("Calificación de experiencia: %s", experience_rating);

    return 0;
}

