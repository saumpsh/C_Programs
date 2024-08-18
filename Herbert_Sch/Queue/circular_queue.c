/*
 * circular_queue.c
 *
 *  Created on: 18-Jan-2018
 *      Author: saum
 *
 *      A circular queue example using a keyboard buffer.
 *      Herbert Schildt
 *
 *      This program will not work in linux since fns "_kbhit"
 *      and "_getch" is only available for windows compiler.
 */


#include <stdio.h>
#include <stdlib.h>

#define MAX 80

char buf[MAX+1];
int spos = 0;
int rpos = 0;

void qstore(char q);
char qretrieve(void);

int main(void)
{
	register char ch;
	int t;

	buf[80] = '\0';

	/* Input characters until a carriage return is typed. */
	for(ch=' ',t=0; t<32000 && ch!='\r'; ++t) {
		if(_kbhit()) {
			ch = _getch();
			qstore(ch);
		}
		printf("%d ", t);
		if(ch == '\r') {
			/* Display and empty the key buffer. */
			printf("\n");
			while((ch=qretrieve()) != '\0') printf("%c", ch);
			printf("\n");
		}
	}
	return 0;
}

/* Store characters in the queue. */
void qstore(char q)
{
	if(spos+1==rpos || (spos+1==MAX && !rpos)) {
		printf("List Full\n");
		return;
	}
	buf[spos] = q;
	spos++;
	if(spos==MAX) spos = 0; /* loop back */
}

/* Retrieve a character. */
char qretrieve(void)
{
	if(rpos==MAX) rpos = 0; /* loop back */
	if(rpos==spos) return '\0';
	rpos++;
	return buf[rpos-1];
}
