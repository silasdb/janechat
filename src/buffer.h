#ifndef JANECHAT_BUFFER_H
#define JANECHAT_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct Buffer Buffer;

/*
 * Fat pointer to our Buffer type.  data length is max+1, because we always make
 * room for the nullbyte, but don't count it in len and max.  Buffer objects are
 * always guaranteed to have null-terminated strings.
 */
struct Buffer {
	char *data;	/* Buffer that store string (always with null byte) */
	size len;	/* UTF-8 length (don't count null byte) */
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
};

inline const char *buffer_data(const Buffer *b) { return b->data; }
inline size_t buffer_len(const Buffer *b) { return b->len; }

Buffer *buffer_new(void);
Buffer *buffer_new_cstr(const char *);
Buffer *buffer_new_bytelen(size_t);
int buffer_len(const Buffer *);
void buffer_append(Buffer *b, const Buffer *s);
void buffer_append_cstr(Buffer *b, const char *s);
void buffer_append_cstr_bytelen(Buffer *b, const char *s, size_t);
int buffer_cmp(const Buffer *, const Buffer *);
void buffer_reset(Buffer *);
void buffer_free(Buffer *);

#endif /* !JANECHAT_BUFFER_H */
