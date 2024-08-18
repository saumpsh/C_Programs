/*
 * main.c
 *
 *  Created on: 15-Jan-2018
 *      Author: saum
 *       Description:
 *  		quick sort algorithm implemented from Kernighan and Ritchie
 *  		pg - 79 ch 4-10
 *  		This program is NOT working as intended
 */
#include <stdio.h>

void qsort(int v[], int left, int right);

int main()
{
	int num[]={4,1,3,2}, i, arr_size;
	arr_size = sizeof(num)/sizeof(int);
	qsort(num, 0, arr_size-1);

	printf("Array after sorting:\n");
	for(i=0;i<arr_size; i++)
		printf("%d\t", num[i]);
	return 1;
}

/* qsort: sort v[left]...v[right] into increasing order */
void qsort(int *v, int left, int right)
{
	int i, last;
	void swap(int *v, int i, int j);

	if (left >= right) /* do nothing if array contains */
		return;	/* fewer than two elements */

	swap(v, left, (left + right)/2); /* move partition elem */
	last = left; 					/* to v[0] */
	for (i = left + 1; i <= right; i++) /* partition */
		if (v[i] < v[left])
			swap(v, ++last, i);
	swap(v, left, last);		   /* restore partition elem */
	qsort(v, left, last-1);
	qsort(v, last+1, right);
}

/* swap: interchange v[i] and v[j] */
void swap(int *v, int i, int j)
{
	int temp;
	temp = v[i];
	v[i] = v[j];
	v[j] = temp;
}
