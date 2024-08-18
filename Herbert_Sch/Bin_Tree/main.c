/*
 * main.c
 *
 *  Created on: 10-Jan-2018
 *      Author: saum
 *
 *      This program displays a binary tree.
 *      Note: press enter again to stop input prompt
 */


#include <stdlib.h>
#include <stdio.h>

struct tree {
	char info;
	struct tree *left;
	struct tree *right;
};

struct tree *root; /* first node in tree */
struct tree *stree(struct tree *root,
		struct tree *r, char info);
void print_tree(struct tree *root, int l);


int main(void)
{
	char s[80];
	root = NULL;  	/* initialize the root */

	do {
		printf("Enter a letter: ");
		gets(s);
		root = stree(root, root, *s);
	} while(*s);
	print_tree(root, 0);
	return 0;
}
struct tree *stree(
		struct tree *root,
		struct tree *r,
		char info)
{
	if(!r) {
		r = (struct tree *) malloc(sizeof(struct tree));

		if(!r) {
			printf ("Out of Memory\n");
					exit(0);
		}

		r->left = NULL;
		r->right = NULL;
		r->info = info;

		if(!root) return r; /* first entry */

		if(info < root->info) root->left = r;
		else root->right = r;
		return r;
	}

	if(info < r->info)
		stree(r, r->left, info);
	else
		stree(r, r->right, info);
	return root;
}

void print_tree(struct tree *r, int l)
{
	int i;

	if(!r) return;

	print_tree(r->right, l+1);
	//print_tree(r->left, l+1);
	for(i=0; i<l; ++i) printf(" ");
	printf("%c\n", r->info);
	print_tree(r->left, l+1);
	//print_tree(r->right, l+1);
}


