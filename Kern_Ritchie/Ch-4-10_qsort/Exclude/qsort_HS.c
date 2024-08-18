/*
 * main2.c
 *
 *  Created on: 15-Jan-2018
 *      Author: saum
 *  Description:
 *  		quick sort algorithm implemented from Herber Schildt
 *  		pg - 508
 */
#include <stdio.h>

void quick(char *items, int count);
void qs(char *items, int left, int right);

int main()
{
	//char num[]={'t', 'b', 'q', 'a'};
	char num[]={'6', '2', '8', '2', '7', '1', '4', '0'};
	int i, arr_size;
	arr_size = sizeof(num);

	quick(num, arr_size);

	printf("Array after sorting:\n");
	for(i=0;i < arr_size; i++)
		printf("%c\t", num[i]);
	printf("\n");
	return 1;
}

void quick(char *items, int count)
{
	qs(items, 0, count-1);
}
/* The Quicksort. */
void qs(char *items, int left, int right)
{
	register int i, j;
	char x, y;

	i = left; j = right;
	x = items[(left+right)/2];
	do {
		while((items[i] < x) && (i < right)) i++;
		while((x < items[j]) && (j > left)) j--;

		if(i <= j) {
			y = items[i];
			items[i] = items[j];
			items[j] = y;
			i++; j--;
		}
	} while(i <= j);

	if(left < j) qs(items, left, j);
	if(i < right) qs(items, i, right);
}
