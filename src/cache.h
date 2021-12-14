#ifndef CACHE_H
#define CACHE_H

void cache_set(const char *key, const char *value);
char *cache_get_alloc(const char *key);
void cache_set_profile(const char *p);

#endif
