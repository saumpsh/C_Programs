#include <stdio.h>
#include <string.h>

#define MAXLINES 50
int MAXLEN = 100;

char *lineptr[MAXLINES];

int readlines(char *lineptr[], int maxlines)
{
    int nlines, len;
    char *p, line[MAXLEN];

    nlines = 0;
    len = getline(&line, &MAXLEN, stdin);
    while(len > 0)
    {
       line[len-1] = '\0';
       strcpy(p, line);
       lineptr[nlines++] = p;
       //len = getline(line, stdin);
    }

    return nlines;
}

int main(int argc, char const *argv[])
{
    freopen("input.txt", "r", stdin);
    int nlines;

    if((nlines = readlines(lineptr, MAXLINES))>= 0)
    {
        //writelines(lineptr, nlines);
    }
    
    return 0;
}
