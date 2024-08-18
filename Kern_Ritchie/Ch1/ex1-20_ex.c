// Detab program to replace tabs with Tab 
#include <stdio.h>

void main()
{
    char c;         //hold a character
    char space = ' ';     //holds space character
    int cnt;      //count number of tabs after a letter
    int i;
    while((c = getchar()) != EOF)
    {
        if(c == '\t')
        {            
            for(i=1; i<=4; i++)  //Print 4 spaces for each tab
                putchar(space);
        }
        else
            putchar(c);
    }
    printf("\n");
}

/*
Posiible input:
saumil  shah        parag
kaushil shah    grandson  Pravin    Shah
*/