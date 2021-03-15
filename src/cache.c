#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cache.h"
#include "utils.h"

/*
 * We provide a very simple mechanism for persistence.  It is simple a key-value
 * database, where each key-value pair is a file, where key is the filename and
 * value its contents.
 
 * Although it is possible, these files are not meant to be edited from an
 * editor.  They do not have a end of line character at the end of the file and
 * editing them can break janechat.
 */

static char *cache_dir();
static void mkdir_r(const char *);

const char *cache_set(const char *key, const char *value) {
	mkdir_r(cache_dir());
	
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", cache_dir(), key);

	FILE *f = fopen(path, "w");
	assert(f != NULL); /* TODO: handle this case */
	fprintf(f, "%s", value);
	fflush(f);
	fclose(f);
	return value;
}

char *cache_get(const char *key) {
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", cache_dir(), key);
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char *contents = read_file(f);
	fclose(f);
	return contents;
}

/*
 * Return a internal pointer to the cache directory.  This pointer is a static
 * object and should not be freed.
 */
static char *cache_dir() {
	static char dir[PATH_MAX] = { '\0' };

	if (dir[0] != '\0')
		return dir;

	if (getenv("XDG_CACHE_HOME")) {
		snprintf(dir, sizeof(dir), "%s/janechat",
			getenv("XDG_CACHE_HOME"));
		return dir;
	}

	assert(getenv("HOME") != NULL);
	snprintf(dir, sizeof(dir), "%s/.cache/janechat", getenv("HOME"));
	return dir;
}

/*
 * Creates a directory tree.  It does that from the bottom-up, by checking if
 * leaf exist before going to the parent.  It creates the parents recursivelly
 * to create the leafs.
 */
static void mkdir_r(const char *path) {
	struct stat s;
	if (stat(path, &s) == 0) {
		if (s.st_mode & S_IFDIR)
			return;
		else
			assert(0 == 1); /* TODO: what if it is a non-directory? */
	}
	
	char parent[PATH_MAX];
	snprintf(parent, sizeof(parent), "%s", path);
	char *c = strrchr(parent, '/');
	*c = '\0';
	mkdir_r(parent);
	if (mkdir(path, 0700) != 0) {
		perror("mkdir_r()"); /* TODO: handle errors correctly */
		exit(1);
	}
}

