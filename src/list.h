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

List *list_new(void);
void list_append(List *, void *);
void *list_pop_head(List *);

/**
 * Traverse all itens of of a list.  At each iteration of the loop, variable
 * `iter` is set to the current item.
 */
#define LIST_FOREACH(l, iter)						\
	for (void *list_entry_##iter = l->head;			\
	 list_entry_##iter &&						\
	  (iter = ((struct list_node *)list_entry_##iter)->val); 	\
	 list_entry_##iter = ((struct list_node *)list_entry_##iter)->next)

#endif /* !JANECHAT_LIST_H */
