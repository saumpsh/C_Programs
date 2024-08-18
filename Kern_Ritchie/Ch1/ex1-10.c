#include <stdio.h>

void main(){

    char c;
    while((c = getchar()) != EOF){
        if(c == '\t')
            c='\\';
        else if(c == '\b')
            c='\\';
        putchar(c);
    }
    
}

