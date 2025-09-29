#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//Si se ingresa un número como parametro al llamar al ejecutable, solo se suma hasta ese número. De lo contrario se hace hasta 1000.
int main(int argc, char *argv[]) {
	int maximo = 1000;
	if(argc >= 2){
		int cambio = atoi(argv[1]);
		if (cambio != 0){
			maximo = cambio;
		} 
	}
	//printf("%d", maximo);
	//Número de letras de los números del 1 al 9
	int unidades[] = {strlen("one"), strlen("two"), strlen("three"), strlen("four"), strlen("five"), strlen("six"), strlen("seven"), strlen("eight"), strlen("nine")};
	//Número de letras de los números del 11 al 19
	int oncea19[] = {strlen("eleven"), strlen("twelve"),strlen("thirteen"),strlen("fourteen"), strlen("fifteen"), strlen("sixteen"), strlen("seventeen"), strlen("eighteen"), strlen("nineteen")};
	//Número de letras de las decenas 10 a la 90
	int decenas[] = {strlen("ten"), strlen("twenty"), strlen("thirty"), strlen("forty"), strlen("fifty"), strlen("sixty"), strlen("seventy"), strlen("eighty"), strlen("ninety")};
	
	int suma = 0;
	for(int i = 1; i < maximo + 1; i++){
		//Sacamos el valor de los miles, centenas, decenas y unidades para saber que valor de los anteriores arreglos llamar
		int miles = i/1000;
		int centenar = i%1000/100;
		int decena = i%100/10;
		int unidad = i%10;
		//A los miles les sumamos también el número de letras de thousand 
		if(miles > 0){
			suma += unidades[miles-1]+ strlen("thousand");
		}
		//A las centenas les sumamos también el número de letras de hundred
		if(centenar > 0){
			suma += unidades[centenar-1]+strlen("hundred");
			//Si tienen decenas o unidades, en Inglaterra se suele agregar un and antes de estos por lo tanto sumamos su valor
			if(decena > 0 || unidad > 0){
				suma += strlen("and");
			}
		}
		
		if(decena > 0){
			//Si los dos lugares restantes esta entre el 11 al 19 sumamos el valor correspondiente indicado en el arreglo oncea19.
			//Por el contrario sumamos el número de la decena correspondiente y la unidad.
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
