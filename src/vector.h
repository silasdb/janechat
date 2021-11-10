#ifndef JANECHAT_VECTOR_H
#define JANECHAT_VECTOR_H

#include <unistd.h>

struct Vector {
	void **elems;
	size_t len;
	size_t max;
};

typedef struct Vector Vector;

Vector *vector_new();
void vector_append(Vector *, void *);

#define VECTOR_FOREACH(v, iter, i) \
	for (i = 0; (i < v->len) && (iter = v->elems[i]); i++)

#endif /* !JANECHAT_VECTOR_H */
