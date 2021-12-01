/*
 * @Description: 
 * @Version: 2.0
 * @Autor: elfred
 * @Date: 2021-11-30 21:32:20
 */

#include <stdio.h>

int fibonacci(int i) {
    if (i <= 1) {
        return 1;
    }
    return fibonacci(i-1) + fibonacci(i-2);
}

int main()
{
    int i;
    i = 0;
    while (i <= 10) {
        printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
        i = i + 1;
    }
    return 0;
}
