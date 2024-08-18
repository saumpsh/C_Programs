/*
 * main.c
 *
 *  Created on: 12-Jan-2018
 *      Author: saum
 *  A simple mailing list program that illustrates the
	use and maintenance of doubly linked lists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct address {
	char name[30];
	char street[40];
	char city[20];
	char state[3];
	char zip[11];
	struct address *next; 	/* pointer to next entry */
	struct address *prior; /* pointer to previous record */
};

struct address *start; /* pointer to first entry in list */
struct address *last; /* pointer to last entry */
struct address *find(char *);

void enter(void), search(void), save(void);
void load(void), list(void);
void mldelete(struct address **, struct address **);
void dls_store(struct address *i, struct address **start,
		struct address **last);

void inputs(char *, char *, int), display(struct address *);
int menu_select (void);

int main(void)
{
	start = last = NULL; 	/* initialize start and end pointers */

	for(;;) {
		switch (menu_select ()) {
			case 1: enter(); /* enter an address */
				break;
			case 2: mldelete(&start, &last); /* remove an address */
				break;
			case 3: list(); /* display the list */
				break;
			case 4: search(); /* find an address */
				break;
			case 5: save(); /* save list to disk */
				break;
			case 6: load(); /* read from disk */
				break;
			case 7: exit(0);
		}
	}
	return 0;
}

/* Select an operation. */
int menu_select (void)
{
	char s[80];
	int c;
	printf("1. Enter a name\n");
	printf("2. Delete a name\n");
	printf("3. List the file\n");
	printf("4. Search\n");
	printf("5. Save the file\n");
	printf("6. Load the file\n");
	printf("7. Quit\n");
	do {
		printf("\nEnter your choice: ");
		gets(s);
		c = atoi(s);
	} while(c<0 || c>7);
	return c;
}

/* Enter names and addresses. */
void enter(void)
{
	struct address *info;
	for(;;) {
		info = (struct address *)malloc(sizeof(struct address));
		if(!info) {
			printf("\nout of memory");
			return;
		}
		inputs("Enter name: ", info->name, 30);
		if(!info->name[0]) break; /* stop entering */
		inputs("Enter street: ", info->street, 40);
		inputs("Enter city: ", info->city, 20);
		inputs("Enter state: ", info->state, 3);
		inputs("Enter zip: ", info->zip, 10);
		dls_store
		(info, &start, &last);
	} /* entry loop */
}

/* This function will input a string up to
 * the length in count and will prevent
the string from being overrun. It will also
display a prompting message. */
void inputs(char *prompt, char *s, int count)
{
	char p[255];

	do {
		printf (prompt);
		fgets (p, 254, stdin);
		if(strlen(p) > count) printf("\nToo Long\n");
	} while (strlen(p) > count);

	p[strlen(p) - 1] = 0; /* remove newline character */
	strcpy(s, p);
}

/* Create a doubly linked list in sorted order. */
void dls_store(
		struct address *i, 	/* new element */
		struct address **start, /* first element in list */
		struct address **last /* last element in list */
		)
{
	struct address *old, *p;

	if(*last==NULL) { /* first element in list */
		i->next = NULL;
		i->prior = NULL;
		*last = i;
		*start = i;
		return;
	}
	p = *start; /* start at top of list */

	old = NULL;
	while (p) {
		if(strcmp(p->name, i->name)<0)
		{
			old = p;
			p = p->next;
		}
		else {
			if(p->prior) {
				p->prior->next = i;
				i->next = p;
				i->prior = p->prior;
				p->prior = i;
				return;
			}
			i->next = p; /* new first element */
			i->prior = NULL;
			p->prior = i;
			*start = i;
			return;
		}
	}
	old->next = i; /* put on end */
	i->next = NULL;
	i->prior = old;
	*last = i;
}

/* Remove an element from the list. */
void mldelete(struct address **start, struct address **last)
{
	struct address *info;
	char s[80];

	inputs("Enter name: ", s, 30);
	info = find(s);
	if(info) {
		if(*start==info) {
			*start=info->next;
			if(*start) (*start)->prior = NULL;
			else *last = NULL;
		}
		else {
			info->prior->next = info->next;
			if(info!=*last)
				info->next->prior = info->prior;
			else
				*last = info->prior;
		}
		free(info); /* return memory to system */
	}
}

/* Find an address. */
struct address *find( char *name)
{
	struct address *info;
	info = start;
	while(info) {
		if(!strcmp(name, info->name)) return info;
		info = info->next; /* get next address */
	}
	printf("Name not found.\n");
		return NULL; /* not found */
}

/* Display the entire list. */
void list(void)
{
	struct address *info;
	info = start;
	while (info) {
		display (info);
		info = info->next;
	}
	printf("\n\n");
	/* get next address */
}
/* This function actually prints the fields in each address. */
void display(struct address *info)
{
	printf("%s\n", info->name);
	printf("%s\n", info->street);
	printf("%s\n", info->city);
	printf("%s\n", info->state);
	printf("%s\n", info->zip);
	printf("\n\n");
}

/* Look for a name in the list. */
void search(void)
{
	char name[40];
	struct address *info;
	printf("Enter name to find: ");
	gets(name);
	info = find(name);
	if(!info)
		printf("Not Found\n");
	else display(info);
}

/* Save the file to disk. */
void save(void)
{
	struct address *info;
	FILE *fp;
	fp = fopen("mlist", "wb");
	if(!fp) {
		printf("Cannot open file.\n");
		exit(1);
	}
	printf("\nSaving File\n");
	info = start;
	while(info) {
		fwrite(info, sizeof(struct address), 1, fp);
		info = info->next; /* get next address */
	}
	fclose(fp);
}


/* Load the address file. */
void load()
{
	struct address *info;
	FILE *fp;

	fp = fopen("mlist", "rb");
	if(!fp) {
		printf("Cannot open file.\n");
		exit (1);
	}

	/* free any previously allocated memory */
	while(start) {
		info = start->next;
		free (info);
		start = info;
	}

	/* reset top and bottom pointers */
	start = last = NULL;

	printf("\nLoading File\n");
	while(!feof(fp)) {
		info = (struct address *) malloc(sizeof(struct address));
		if(!info) {
			printf("Out of Memory");
					return;
		}
		if(1 != fread(info, sizeof(struct address), 1, fp)) break;
		dls_store(info, &start, &last);
	}
	fclose (fp);
}

