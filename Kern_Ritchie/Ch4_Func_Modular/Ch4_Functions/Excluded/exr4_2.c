/*
 * sample_2.c
 *
 *  Created on: 11-Dec-2017
 *      Author: saum
 */

#include <ctype.h>
#include <stdio.h>
#define MAXLINE 100

/* atof: convert string s to double */
double atof(char s[])
{
	double val, power, power2;
	int i, sign, sign2;
	for (i = 0; isspace(s[i]); i++) /* skip white space */
		;
	sign = (s[i] == '-') ? -1 : 1;
	if (s[i] == '+' || s[i] == '-')
		i++;
	for (val = 0.0; isdigit(s[i]); i++)
		val = 10.0 * val + (s[i] - '0');
	if (s[i] == '.')
		i++;
	for (power = 1.0; isdigit(s[i]); i++) {
		val = 10.0 * val + (s[i] - '0');
		power *= 10;
	}
	if (s[i] == 'e' || s[i] == 'E')
		i++;
	sign2 = (s[i] == '-') ? -1 : 1;
	if (s[i] == '+' || s[i] == '-')
		i++;
	if(isdigit(s[i]))
	{
		int j; power2 = 1;
		for (j = 1; j<= (s[i] - '0'); j++) {
			power2 *= 10;
		}
		switch (sign2)
		{
		case 1:
			val = val * power2;
			break;
		case -1:
			val = val / power2;
		}
	}

	return sign * val / power;
}

/* rudimentary calculator */
int main()
{
	double sum, atof(char []);
	char line[MAXLINE];
	int get_line(char line[], int max);
	sum = 0;
	while (get_line(line, MAXLINE) > 0)
		printf("\t%g\n", /*sum += */atof(line));
	return 0;
}

/* getline: get line into s, return length */
int get_line(char s[], int lim)
{
	int c, i;
	i = 0;
	while (--lim > 0 && (c=getchar()) != EOF && c != '\n')
		s[i++] = c;
	if (c == '\n')
		s[i++] = c;
	s[i] = '\0';
	return i;
}
