#include <stdio.h>
#define MAXLINE 1000  /* MAXiumum input line length */

int getline(char line[], int maxline);
void copy(char to[], char from[]);

/*print the longest input line*/
int main()
{
    int len; //current line length
    int max;  //maximum length so far
    int count; //Line number ie (1..2...3)
    char line[MAXLINE];     // current input line    
    char longest[MAXLINE]; //longest line saved here

    max=0; count = 1;
    while((len = getline(line, MAXLINE))>0)
    {
        printf("Lenght of Line %d : %d\n",count, len);
        count++;
        if(len>80){
            printf("Long line : %s\n",line);
            copy(longest, line);
        }
    }
    return 0;
}

/*getline: read a line into s , return length */
int getline(char s[], int lim)
{
    int c,i;

    for(i=0; i<lim-1 && (c=getchar())!=EOF && c!='\n'; ++i)
        s[i] = c;
    if(c== '\n'){
        s[i] = c;
        ++i;
    }
    s[i] = '\0';
    return i;
}

/*copy: copy 'from' into 'to'; assume to is big enough */
void copy(char to[], char from[])
{
    int i;

    i=0;
    while ((to[i] = from[i]) != '\0')
        ++i;
}

/*
Posiible input:
saumil shah      
pravin  paraag alap   dinesh
tanaji ram fasldjf dalfjasdlf  fasfweotugnasf 
*/