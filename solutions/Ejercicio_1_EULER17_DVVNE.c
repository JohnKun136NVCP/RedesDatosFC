#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
	int maximo = 1000;
	if(argc >= 2){
		int cambio = atoi(argv[1]);
		if (cambio != 0){
			maximo = cambio;
		} 
	}
	//printf("%d", maximo);
	
	int unidades[] = {strlen("one"), strlen("two"), strlen("three"), strlen("four"), strlen("five"), strlen("six"), strlen("seven"), strlen("eight"), strlen("nine")};
	int oncea19[] = {strlen("eleven"), strlen("twelve"),strlen("thirteen"),strlen("fourteen"), strlen("fifteen"), strlen("sixteen"), strlen("seventeen"), strlen("eighteen"), strlen("nineteen")};
	int decenas[] = {strlen("ten"), strlen("twenty"), strlen("thirty"), strlen("forty"), strlen("fifty"), strlen("sixty"), strlen("seventy"), strlen("eighty"), strlen("ninety")};
	
	int suma = 0;
	for(int i = 1; i < maximo + 1; i++){
		int miles = i/1000;
		int centenar = i%1000/100;
		int decena = i%100/10;
		int unidad = i%10;
		
		if(miles > 0){
			suma += unidades[miles-1]+ strlen("thousand");
		}
		
		if(centenar > 0){
			suma += unidades[centenar-1]+strlen("hundred");
			if(decena > 0 || unidad > 0){
				suma += strlen("and");
			}
		}
		
		if(decena > 0){
			if(decena == 1 && unidad != 0){
				suma += oncea19[unidad-1];
			}
			else{
				suma += decenas[decena-1];
			}
		}
		
		if(decena != 1 && unidad > 0){
			suma += unidades[unidad-1];
		}
	}
	printf("%d", suma);
}
