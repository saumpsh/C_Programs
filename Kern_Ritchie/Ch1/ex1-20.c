#include <stdio.h>
#define MAXLINE 1000  /* MAXiumum input line length */

int xgetline(void);
void copy(void);

int max;  //maximum length so far
char line[MAXLINE];     // current input line    
char longest[MAXLINE]; //longest line saved here


/*print the longest input line*/
int main()
{
    int len; //current line length
    
    max=0;
    while((len = xgetline())>0)
        if(len>max){
            max = len;
            copy();
        }
    if(max>0) //there was a line
        printf("%s\n",longest);
    return 0;
}

/*xxgetline: read a line into s , return length */
int xgetline(void)
{
    int c,i;

    for(i=0; i<MAXLINE-1 && (c=getchar())!=EOF && c!='\n'; ++i)
        line[i] = c;
    if(c== '\n'){
        line[i] = c;
        ++i;
    }
    line[i] = '\0';
    return i;
}

/*copy: copy 'from' into 'to'; assume to is big enough */
void copy(void)
{
    int i;

    i=0;
    while ((longest[i] = line[i]) != '\0')
        ++i;
}

/*
Posiible input:
saumil shah      
pravin  paraag alap   dinesh
tanaji ram fasldjf dalfjasdlf  fasfweotugnasf
fdjlfas lutrap  afa  
*/