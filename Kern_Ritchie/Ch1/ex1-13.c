#include <stdio.h>

/* count digits, white space, others */

/*void main()
{
    int c, i, nwhite, nother;
    int ndigit[10];

    nwhite = nother = 0;
    for (i = 0; i < 10; ++i)
        ndigit[i] = 0;

    while ((c = getchar()) != EOF)
        if (c >= '0' && c <= '9')
            ++ndigit[c-'0'];
        else if (c == ' ' || c == '\n' || c == '\t')
            ++nwhite;
        else
            ++nother;

    printf("digits =");
    for (i = 0; i < 10; ++i)
        printf(" %d", ndigit[i]);
    printf(", white space = %d, other = %d\n",
    nwhite, nother);
}*/

/*
Possible input:
asdfh793 hfa     03756-354 f 
afd -37 h
     wejf   jiofeur -57 nbar4w 3riu

*/

//print histogram 

#define IN 1   /* inside a world */
#define OUT 0 /* outside a word */

void main()
{
    int c, i, nc, state;
    int ndigit[10];

    state = OUT;
    nc = 0;
    
    for (i = 0; i < 10; ++i)
        ndigit[i] = 0;

    while ((c = getchar()) != EOF)

       if(c == ' ' || c == '\n' || c == '\t'){
            if(state == IN)
                ++ndigit[nc];  
            state = OUT;
            nc = 0;
        }
            
        else{
            state = IN;
            ++nc;
        } 

    printf("HISTOGRAM =\n");
    for (i = 0; i < 10; ++i)
        printf("No of %d characters : %d\n", i, ndigit[i]);
    
    
}

