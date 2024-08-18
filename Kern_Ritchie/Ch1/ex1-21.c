// Entab that replaces strings of blanks by the minimum
//number of tabs and blanks to achieve the same spacing
//Assume tab stop = 4
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void main()
{
    char c;         //hold a character
    char space = ' ';     //holds space character
    int cnt;      //count number of tabs after a letter
    int i;
    while((c = getchar()) != EOF)
    {
        if(c == ' ')
        {
            while((c = getchar()) == ' ' && (c = getchar()) != EOF)
                cnt ++;
            for(i=0; i<(cnt/4); i++)
                putchar('\t');               
            for(i=0; i<(cnt%4); i++)
                putchar(' ');
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