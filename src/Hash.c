#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "Hash.h"

#define HASH_SIZE 256
struct node {
	const char *key;
	const void *val;
	void *next;
};

struct Hash {
	struct node *table[HASH_SIZE];
};

/* TODO: document why not 
typedef struct Hash Hash[HASH_SIZE];
*/

// test comment 2

static size_t hash_calculate_idx(const char *);

Hash *
hash_new()
{
	Hash *h = malloc(sizeof(Hash));
	memset(h->table, 0x0, sizeof(h->table));
	return h;
}

void
hash_insert(Hash *h, const char *key, const void *val)
{
	size_t idx = hash_calculate_idx(key);
	if (h->table[idx] == NULL) {
		h->table[idx] = (struct node *)malloc(sizeof(struct node));
		h->table[idx]->key = key;
		h->table[idx]->val = val;
		h->table[idx]->next = NULL;
		return;
	}
	struct node *node;
	node = h->table[idx];
	while (node->next != NULL) {
		node = node->next;
		assert(strcmp(node->key, key) != 0); // TODO: overwrite if it happens?
	}
	struct node *next = malloc(sizeof(struct node));
	node->next = next;
	next->key = key;
	next->val = val;
	next->next = NULL;
}

const void *
hash_get(const Hash *h, const char *key)
{
	size_t idx = hash_calculate_idx(key);
	struct node *node = h->table[idx];
	while (node != NULL) {
		if (strcmp(node->key, key) == 0)
			return node->val;
		node = node->next;
	}
	return NULL;
}

static size_t
hash_calculate_idx(const char *key)
{
	size_t idx;
	idx = 0;
	for (size_t i = 0; i < strlen(key); i++)
		idx += (unsigned char)key[i];
	idx %= HASH_SIZE;
	return idx;
}

