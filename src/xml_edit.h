#ifndef PYTHON_STUDIO_XML_EDIT_H
#define PYTHON_STUDIO_XML_EDIT_H
#include "buffer.h"
#include "completion.h"

#define XML_EDIT_DEPTH 128
#define XML_EDIT_NAME 128
#define XML_EDIT_NAMES 64
#define XML_EDIT_EVENTS 32768

typedef struct {
    size_t start, end, cursor; /* UTF-8 bytes, cursor absolute AFTER replacement. */
    char *text;                /* Caller frees, including empty skip plans. */
} XmlEdit;

bool xml_document_name(const char *name);
bool xml_edit_enabled(const Buffer *b);
/* Pure, bounded lexical assistance for incomplete XML. No parser, file or
   network access. -1 allocation/limit error, 0 no special action, 1 plan. */
int xml_edit_type(const char *text, size_t len, size_t cursor, unsigned c, XmlEdit *out);
int xml_edit_newline(const char *text, size_t len, size_t cursor, XmlEdit *out);
int xml_edit_completion(const char *text, size_t len, size_t cursor, const char *name, XmlEdit *out);
/* -1 outside an XML tag-name context; otherwise 0..capacity suggestions. */
int xml_tag_completions(const char *text, size_t len, size_t cursor, CompletionItem *out, int capacity);
bool xml_edit_apply(Buffer *b, const XmlEdit *edit);
#endif
