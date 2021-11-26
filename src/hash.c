#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "list.h"
#include "str.h"

#define HASH_SIZE 256

/**
 * This file implements a generic hash table.
 *
 * This Hash implementation has a total of HASH_SIZE buckets.  Each bucket is
 * the head of a linked list and whever there is a hash collision, the new item
 * is appended to the list.
 *
 * An illustration on how this hash table exists:
 *
 * +-------------+  +---+
 * |      1      |->|   |->NULL
 * +-------------+  +---+
 * +-------------+  +---+  +---+
 * |      2      |->|   |->|   |->NULL
 * +-------------+  +---+  +---+
 *      ...
 * +-------------+
 * | HASH_SIZE-1 |
 * +-------------+
 *
 * Now, the relationship between data structures in this file:
 *
 * The user passes to `hash_insert()` a `val` (`void *`).  The object (actually,
 * the pointer) is wrapped around a `struct hash_item`.  The sole purpose of
 * this struct is to hold `val` alongside with `key` and we need to make that
 * because there can be hash collisions.  When that happens, we need to traverse
 * the list of `struct hash_item`, comparing the keys to fetch the correct item.
 *
 * So, relationship between data structures, from the container to the
 * contained, can be summarized as: Hash > List > struct list_node (declared in
 * list.h) > struct hash_item > val.
 */

struct hash_item {
	const char *key;
	void *val;
};

struct Hash {
	List *table[HASH_SIZE];
};

/* TODO: document why not 
typedef struct Hash Hash[HASH_SIZE];
*/

// test comment 2

static size_t hash_calculate_idx(const char *);

Hash *hash_new(void) {
	Hash *h = malloc(sizeof(Hash));
	memset(h->table, 0x0, sizeof(h->table));
	return h;
}

void hash_insert(Hash *h, const char *key, const void *val) {
	size_t idx = hash_calculate_idx(key);
	if (!h->table[idx])
		h->table[idx] = list_new();
	List *l = h->table[idx];
	struct hash_item *iter;
	LIST_FOREACH(l, iter) {
		// TODO: overwrite if it happens?
		assert(!streq(iter->key, key));
	}
	struct hash_item *item = malloc(sizeof(struct hash_item));
	item->key = key;
	item->val = (void *)val;
	list_append(l, item);
}

void *hash_get(const Hash *h, const char *key) {
	size_t idx = hash_calculate_idx(key);
	List *l = h->table[idx];
	if (!l)
		return NULL;
	struct hash_item *item;
	LIST_FOREACH(l, item) {
		if (streq(item->key, key))
			return item->val;
	}
	return NULL;
}

static size_t hash_calculate_idx(const char *key) {
	size_t idx;
	idx = 0;
	for (size_t i = 0; i < strlen(key); i++)
		idx += (unsigned char)key[i];
	idx %= HASH_SIZE;
	return idx;
}

