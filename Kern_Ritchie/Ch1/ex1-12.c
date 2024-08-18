#include <stdio.h>


void main(){

    int c, cnt = 0;
    
    while((c = getchar()) != EOF){
        
        if(c == ' ' || c == '\n' || c == '\t'){
            cnt++;
            if(cnt == 1)
                putchar('\n');
        }
        else
        {
            putchar(c);
            
            cnt++;
            cnt = 0;       
        }
    }
}

/*

Possible input stream
saumil	shah
parag 	pravin rajiv			shah
*/

