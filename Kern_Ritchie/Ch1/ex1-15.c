#include <stdio.h>


// Finding Exponent of a Number

//int power(int m, int n);

/*test power function*/
/*void main()
{
    int i;
    for (i = 0; i<10; i++){
        printf("%d %d %d\n", i, power(2,i), power(-3,i));
    
    }
}*/

/* power: raise base to n-th power; n>= 0 */
/*int power(int base, int n)
{
    int i, p;

    p=1;
    for(i = 1; i<=n; i++){
        p = p * base;
    }
    return p;
}*/

// Tempetature conversion program using function

float tempetature(int);

void main()
{
    int fahr;
    for (fahr = 0; fahr<=300; fahr = fahr + 20){
        printf("%3.0d %18.2f\n", fahr, tempetature(fahr));
    
    }
}

float tempetature(int fahr){

    float celsius;
    celsius = (5.0/9.0) * (fahr-32.0);
    return celsius;

}