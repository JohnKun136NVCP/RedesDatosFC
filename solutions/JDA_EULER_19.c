#include <stdio.h>
#include <stdlib.h> 


int main(){
  char buffer[12];
  /*
   * representa los dias de la semana: 
   * 0 = lunes;
   * 1 = martes;
   * 2 = miercoles;
   * 3 = jueves;
   * 4 = viernes;
   * 5 = sabado;
   * 6 = domingo;
   * */
  int week = 1;
  /*
   * dia del mes: (1-31)
   * */
  int day = 1;
  /*
   * representa los meses:
   * 1 = enero;
   * 2 = febrero;
   * 3 = marzo;
   * 4 = abril;
   * 5 = mayo;
   * 6 = junio;
   * 7 = julio;
   * 8 = agosto;
   * 9 = septiembre;
   * 10 = octubre;
   * 11 = noviembre;
   * 12 = diciembre;
   * */
  int month = 1;
  /*
   * representa en que año estamos 1900-2000
   * */
  int year = 1901;
  /*
   * contador de 1ro de mes que es domingo
   * */
  int counter = 0;
  
  
  
  for (year; year < 2001; year++){
    month = 1;
    for(month; month < 13; month++){
      day = 1;
      for (day; day < 32; day++){
        if(week > 6){
          week = 0;
        }
        if ((month == 2 && day > 28)&& !((year % 4 == 0) && day > 28 && day < 30)){
          continue;
        }
        if ((month == 9 || month == 4 || month == 6 || month == 11) && day > 30){
          continue;
        }

        if (day == 1 && week == 6){
          counter = counter + 1;
        }
        week = week + 1;

      }
    }
  }

  printf("el numero de domingos que son 1ro de mes es: ");
  sprintf(buffer, "%d", counter);
  printf(buffer);
  return 0;
  
}
