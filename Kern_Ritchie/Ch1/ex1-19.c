#include <stdio.h>
#define MAXLINE 1000  /* MAXiumum input line length */

int getline(char line[], int maxline);
void copy(char to[], char from[]);
void reverse(char line_orig[], char rev[], int line_len);

/*print the longest input line*/
int main()
{
    int len; //current line length
    int max;  //maximum length so far
    int count; //Line number ie (1..2...3)
    char line[MAXLINE];     // current input line    
    char rev_line[MAXLINE]; //Reversed line saved here

    max=0; count = 1;
    while((len = getline(line, MAXLINE))>0)
    {
        reverse(line, rev_line, len);
        printf("%s\n",rev_line);
        
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

//fucntion to reverse a line
void reverse(char line_orig[], char rev[], int line_len)
{
    int i;
    for(i=0;i<(line_len-2);i++)
    {
        rev[i]= line_orig[(line_len-2)-i];
    }
    rev[i+1] = '\0';
}

/*
Posiible input:
saumil shah      
pravin  paraag alap   dinesh
tanaji ram fasldjf dalfjasdlf  fasfweotugnasf 
*/