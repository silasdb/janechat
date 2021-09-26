#ifndef JANECHAT_LIST_H
#define JANECHAT_LIST_H

struct list_node {
	void *val;
	struct list_node *next;
};

struct List {
	struct list_node *head;
	struct list_node *tail;
};

typedef struct List List;

List *list_new();
void list_append(List *, void *);
void *list_pop_head(List *);

/**
 * Traverse all itens of of a list.  At each iteration of the loop, variable
 * `iter` is set to the current item.
 *
 * Implementation details: We cannot declare variables of different types in the
 * initializer part of the loop.  A possible alternative is to declare `void
 * *iter` just before loop and then declare `struct list_node *list_entry##iter`
 * within the initialization part, or the other way around.  To avoid having a
 * variable declared after the loop finishes, we chose to declare both variables
 * at the initialization part as `void *` and perform some casting in parts of
 * the loop that perform evaluation.
 */
#define LIST_FOREACH(l, iter)						\
	for (void *iter, *list_entry_##iter = l->head;			\
	 list_entry_##iter &&						\
	  (iter = ((struct list_node *)list_entry_##iter)->val); 	\
	 list_entry_##iter = ((struct list_node *)list_entry_##iter)->next)

#endif /* !JANECHAT_LIST_H */
