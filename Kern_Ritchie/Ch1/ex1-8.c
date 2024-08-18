#include <stdio.h>

//character counting
/*void main(){

    long nc;

    nc = 0;
    while (getchar() != EOF)
        ++nc;
    printf("%ld\n", nc);
}*/

//line counting
/*void main(){

    int c, nl;

    nl = 0;
    while ((c = getchar()) != EOF)
        if(c == '\n')
            ++nl;
    printf("%ld\n", nl);
}*/


void main(){

    int blank,tab,nl ;
    char c;
    while((c = getchar()) != EOF){
        if(c == '\n')
            ++nl;
        if(c == ' ')
            ++blank;
        if(c == '\t')
            ++tab;
    }
    printf("New Line:%d\nBlank:%d\nTab:%d\n",nl,blank,tab);
}