#include <assert.h>
#include <stdlib.h>

#include "vector.h"

#define INITSIZE 256

Vector *vector_new(void) {
	Vector *v;
	v = malloc(sizeof(struct Vector)); 
	v->elems = malloc(sizeof(void *) * INITSIZE);
	v->len = 0;
	v->max = INITSIZE;
	return v;
}

void vector_append(Vector *v, void *elem) {
	if (v->len == v->max) {
		v->max *= 2;
		v->elems = realloc(v->elems, (sizeof(void *) * v->max));
	}
	v->elems[v->len] = elem;
	v->len++;
}

void vector_sort(Vector *v, int (*compar)(const void *, const void *)) {
	qsort(v->elems, v->len, sizeof(void *), compar);
}
