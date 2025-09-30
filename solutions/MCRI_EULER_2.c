#include <stdio.h>

int main()
{
    int res = 2;
    int fib[100] = {0};
    fib[0] = 1;
    fib[1] = 2;
    for(int i = 2; i < 100; i++){
        fib[i] = fib[i-1] + fib[i-2];
        if(fib[i] < 4000000){
            if(fib[i] % 2 == 0){
                res += fib[i]; 
            }
        }
        else {
            break;
        }
    }
    printf("The sum of the even valued numbers in the Fibonacci sequence below 4000000 is %d", res);
}