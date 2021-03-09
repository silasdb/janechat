#ifndef JANECHAT_HASH_H
#define JANECHAT_HASH_H

typedef struct Hash Hash;

Hash *hash_new();
void hash_insert(Hash *, const char *, const void *);
const void *hash_get(const Hash *, const char *);

#endif /* !JANECHAT_HASH_H */
