#ifndef JANECHAT_STRBUF_H
#define JANECHAT_STRBUF_H

typedef struct StrBuf StrBuf;

char *strbuf_buf(const StrBuf *);
size_t strbuf_len(const StrBuf *);
int strbuf_cmp(const StrBuf *, const StrBuf *);
StrBuf *strbuf_new();
StrBuf *strbuf_new_c(const char *s);
void strbuf_cat_c(StrBuf *ss, const char *s);
void strbuf_ncat_c(StrBuf *, const char *, size_t);
void strbuf_reset(StrBuf *);
void strbuf_incref(StrBuf *);
void strbuf_decref(StrBuf *);
size_t strbuf_len(const StrBuf *);
char *strbuf_detach(StrBuf *);


#endif /* !JANECHAT_STRBUF_H */
