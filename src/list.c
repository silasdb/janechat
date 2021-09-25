#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

static void alloc_head(List *, void *);

List *list_new() {
	List *l = malloc(sizeof(List));
	l->head = NULL;
	l->tail = NULL;
	return l;
}

void list_append(List *l, void *val) {
	if (l->head) {
		struct list_node *n = malloc(sizeof(struct list_node));
		n->val = val;
		n->next = NULL;
		l->tail->next = n;
		l->tail = n;
	} else {
		alloc_head(l, val);
	}
}

void *list_pop_head(List *l) {
	if (!l->head)
		return NULL;
	void *val;
	struct list_node *n;
	n = l->head;
	l->head = l->head->next;
	val = n->val;
	free(n);
	return val;
}

static void alloc_head(List *l, void *val) {
	/* Empty list.  Alocate first object. */
	l->head = malloc(sizeof(struct list_node));
	l->head->val = val;
	l->head->next = NULL;
	l->tail = l->head;
}
