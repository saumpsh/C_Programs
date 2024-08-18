#include <stdio.h>

void main(){

    int blank, state, c, cnt = 0, cnt_c = 0  ;

    while((c = getchar()) != EOF){
        ++cnt_c;
        if(c == ' '){
            cnt = 0;
        }
           
        else{
            if(cnt == 0 )
                if(cnt_c != 1){
                    putchar(' ');
                }
                    
            putchar(c);
            cnt ++;
        }
        
            
       
    }
    
}