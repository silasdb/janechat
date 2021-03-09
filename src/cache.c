#include <limits.h>
#include <stdlib.h>

#include "cache.h"
#include "utils.h"

/*
 * TODO: for now, this is just a quick-&-dirty implementation of what will be a
 * cache mechanism for janechat.
 *
 * Special care with non-existing paths should be taken.
 */

const char *
cache_set(const char *key, const char *value)
{
	(void)key;
	char *home = getenv("HOME");
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/.config/janechat.cache", home);

	FILE *f = fopen(path, "w");
	if (!f)
		return NULL;
	fprintf(f, "%s", value);
	fclose(f);
	return value;
}

char *
cache_get(const char *key)
{
	(void)key;
	char *home = getenv("HOME");
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/.config/janechat.cache", home);
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char *contents = read_file(f);
	fclose(f);
	return contents;
}
