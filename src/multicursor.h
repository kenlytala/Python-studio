#ifndef PRISME_MULTICURSOR_H
#define PRISME_MULTICURSOR_H
#include "buffer.h"
/* Private dispatch used by buffer.c; the public API stays in buffer.h. */
enum {
    MULTI_INSERT,
    MULTI_TYPE,
    MULTI_BACKSPACE,
    MULTI_DELETE,
    MULTI_NEWLINE,
    MULTI_INDENT,
    MULTI_OUTDENT,
    MULTI_COMMENT,
    MULTI_DUPLICATE
};
enum { MULTI_LEFT, MULTI_RIGHT, MULTI_VERTICAL, MULTI_HOME, MULTI_END };
bool multi_edit(Buffer *b, int operation, const char *text, unsigned argument);
void multi_move(Buffer *b, int operation, int argument, bool select);
char *multi_selected(const Buffer *b);
#endif
