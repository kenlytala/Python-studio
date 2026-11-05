#define _POSIX_C_SOURCE 200809L
#include "xml_refs.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#define read _read
#define close _close
#define fstat _fstat64
#define stat _stat64
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#else
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool fail(char *error, size_t cap, const char *text) {
    if (error && cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool cancelled(const atomic_bool *cancel) {
    return cancel && atomic_load(cancel);
}
static bool delimiter(unsigned char c) {
    return isspace(c) || strchr("\"'`<>=()[]{};,|", c) != NULL;
}
static bool candidate(const char *text, size_t start, size_t end, XmlPathSpan *out) {
    while (start < end && isspace((unsigned char)text[start]))
        start++;
    while (end > start && isspace((unsigned char)text[end - 1]))
        end--;
    size_t a = start, b = end;
    if (b - a >= 2 && (text[a] == '\'' || text[a] == '"') && text[b - 1] == text[a]) {
        a++;
        b--;
    }
    size_t n = b - a;
    if (n < 4 || n >= BUF_PATH || memchr(text + a, '\0', n) || memchr(text + a, '\n', n) ||
        memchr(text + a, '\r', n))
        return false;
    const char *ext = text + b - 4;
    if (ext[0] != '.' || tolower((unsigned char)ext[1]) != 'x' || tolower((unsigned char)ext[2]) != 'm' ||
        tolower((unsigned char)ext[3]) != 'l')
        return false;
    memcpy(out->path, text + a, n);
    out->path[n] = 0;
    if (strstr(out->path, "://"))
        return false;
    out->start = start;
    out->end = end;
    return true;
}
bool xml_path_span(const char *text, size_t len, size_t cursor, size_t anchor, XmlPathSpan *out) {
    if (!text || !out || cursor > len || (anchor != BUF_NO_SELECTION && anchor > len))
        return false;
    if (anchor != BUF_NO_SELECTION && anchor != cursor)
        return candidate(text, anchor < cursor ? anchor : cursor, anchor > cursor ? anchor : cursor, out);
    size_t left = cursor, right = cursor;
    while (left && text[left - 1] != '\n')
        left--;
    while (right < len && text[right] != '\n')
        right++;
    for (size_t i = left; i < right; i++)
        if (text[i] == '\'' || text[i] == '"') {
            size_t end = i + 1;
            while (end < right && text[end] != text[i])
                end++;
            if (end == right)
                break;
            if (i <= cursor && cursor <= end + 1 && candidate(text, i, end + 1, out))
                return true;
            i = end;
        }
    size_t end = cursor;
    while (end > left && (text[end - 1] == ' ' || text[end - 1] == '\t'))
        end--;
    size_t start = end;
    while (start > left && !delimiter((unsigned char)text[start - 1]))
        start--;
    while (end < right && !delimiter((unsigned char)text[end]))
        end++;
    return candidate(text, start, end, out);
}
/* Reject a real declaration BEFORE a parser can inspect its DTD. Literal examples
   inside comments, CDATA and processing instructions are not declarations. */
static bool no_doctype(const char *raw, size_t len) {
    enum { MARKUP, COMMENT, CDATA, PI } state = MARKUP;
    for (size_t i = 0; i < len;) {
        size_t left = len - i;
        if (state == COMMENT) {
            if (left >= 3 && !memcmp(raw + i, "-->", 3)) {
                state = MARKUP;
                i += 3;
            } else
                i++;
        } else if (state == CDATA) {
            if (left >= 3 && !memcmp(raw + i, "]]>", 3)) {
                state = MARKUP;
                i += 3;
            } else
                i++;
        } else if (state == PI) {
            if (left >= 2 && !memcmp(raw + i, "?>", 2)) {
                state = MARKUP;
                i += 2;
            } else
                i++;
        } else if (raw[i] != '<')
            i++;
        else if (left >= 9 && !memcmp(raw + i, "<!DOCTYPE", 9))
            return false;
        else if (left >= 4 && !memcmp(raw + i, "<!--", 4)) {
            state = COMMENT;
            i += 4;
        } else if (left >= 9 && !memcmp(raw + i, "<![CDATA[", 9)) {
            state = CDATA;
            i += 9;
        } else if (left >= 2 && !memcmp(raw + i, "<?", 2)) {
            state = PI;
            i += 2;
        } else
            i++;
    }
    return true; /* malformed constructs are rejected by the parser */
}
static void reader_error(void *arg, const char *message, xmlParserSeverities severity,
                         xmlTextReaderLocatorPtr locator) {
    (void)message;
    (void)locator;
    if (arg && (severity == XML_PARSER_SEVERITY_ERROR || severity == XML_PARSER_SEVERITY_VALIDITY_ERROR))
        *(bool *)arg = true;
}
void xml_refs_free(XmlReferences *refs) {
    if (!refs)
        return;
    free(refs->raw);
    free(refs->items);
    memset(refs, 0, sizeof(*refs));
}
bool xml_refs_parse(XmlReferences *out, const char *raw, size_t len, const atomic_bool *cancel, char *error,
                    size_t cap) {
    if (!out || !raw || len > XML_REF_SOURCE_MAX)
        return fail(error, cap, "Fichier XML trop volumineux (8 Mio maximum).");
    XmlReferences refs = {0};
    if (memchr(raw, 0, len) || !utf8_valid(raw, len))
        return fail(error, cap, "Le fichier XML doit être du texte UTF-8, sans NUL.");
    refs.raw = malloc(len + 1);
    if (!refs.raw)
        return fail(error, cap, "Mémoire insuffisante.");
    memcpy(refs.raw, raw, len);
    refs.raw[len] = 0;
    refs.len = len;
    if (!no_doctype(refs.raw, len)) {
        xml_refs_free(&refs);
        return fail(error, cap, "DTD et entités externes XML ne sont pas autorisées.");
    }
    xmlTextReaderPtr reader =
        xmlReaderForMemory(refs.raw, (int)len, NULL, "UTF-8",
                           XML_PARSE_NONET | XML_PARSE_COMPACT | XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
                               XML_PARSE_BIG_LINES | XML_PARSE_IGNORE_ENC);
    if (!reader) {
        xml_refs_free(&refs);
        return fail(error, cap, "XML invalide ou mémoire insuffisante.");
    }
    bool parser_failed = false;
    xmlTextReaderSetErrorHandler(reader, reader_error, &parser_failed);
    bool ok = true;
    int status = 0, capacity = 0;
    unsigned nodes = 0, attributes = 0;
    while ((status = xmlTextReaderRead(reader)) == 1) {
        int type = xmlTextReaderNodeType(reader);
        if (cancelled(cancel)) {
            ok = fail(error, cap, "Lecture XML annulée.");
            break;
        }
        if (++nodes > XML_REF_NODES_MAX ||
            (type == XML_READER_TYPE_ELEMENT && xmlTextReaderDepth(reader) >= XML_REF_DEPTH_MAX)) {
            ok = fail(error, cap, "Structure XML trop profonde ou trop volumineuse.");
            break;
        }
        if (type == XML_READER_TYPE_DOCUMENT_TYPE || type == XML_READER_TYPE_ENTITY_REFERENCE) {
            ok = fail(error, cap, "DTD et entités externes XML ne sont pas autorisées.");
            break;
        }
        if (type != XML_READER_TYPE_ELEMENT)
            continue;
        int attrs = xmlTextReaderAttributeCount(reader);
        if (attrs > 0)
            attributes += (unsigned)attrs;
        if (attributes > XML_REF_NODES_MAX) {
            ok = fail(error, cap, "Structure XML trop volumineuse.");
            break;
        }
        xmlChar *id = xmlTextReaderGetAttribute(reader, BAD_CAST "id");
        if (!id || !*id) {
            xmlFree(id);
            continue;
        }
        xmlChar *model = xmlTextReaderGetAttribute(reader, BAD_CAST "model");
        const xmlChar *tag = xmlTextReaderConstLocalName(reader);
        if (refs.count == XML_REF_IDS_MAX || xmlStrlen(id) >= 1024 || (model && xmlStrlen(model) >= 1024) ||
            !tag || xmlStrlen(tag) >= 128) {
            xmlFree(id);
            xmlFree(model);
            ok =
                fail(error, cap, "Limite XML : 4096 identifiants, id/modèle 1023 octets, balise 127 octets.");
            break;
        }
        if (refs.count == capacity) {
            int next = capacity ? capacity * 2 : 64;
            XmlIdentifier *items = realloc(refs.items, (size_t)next * sizeof(*items));
            if (!items) {
                xmlFree(id);
                xmlFree(model);
                ok = fail(error, cap, "Mémoire insuffisante.");
                break;
            }
            refs.items = items;
            capacity = next;
        }
        XmlIdentifier *item = &refs.items[refs.count];
        memset(item, 0, sizeof(*item));
        strcpy(item->id, (const char *)id);
        strcpy(item->tag, (const char *)tag);
        if (model)
            strcpy(item->model, (const char *)model);
        item->ordinal = (unsigned)refs.count++;
        const xmlNode *node = xmlTextReaderCurrentNode(reader);
        item->line = node ? xmlGetLineNo(node) : xmlTextReaderGetParserLineNumber(reader);
        xmlFree(id);
        xmlFree(model);
    }
    if (ok && (status < 0 || parser_failed || cancelled(cancel)))
        ok = fail(error, cap,
                  cancelled(cancel) ? "Lecture XML annulée."
                                    : "XML invalide : vérifiez les balises et l’encodage UTF-8.");
    xmlFreeTextReader(reader);
    if (!ok) {
        xml_refs_free(&refs);
        return false;
    }
    *out = refs;
    return true;
}
static xmlChar *plain_prop(xmlNodePtr node, const xmlChar *name) {
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
        if (!attr->ns && xmlStrEqual(attr->name, name))
            return xmlNodeListGetString(node->doc, attr->children, 1);
    return NULL;
}
static bool arch_field(xmlNodePtr node) {
    if (node->type != XML_ELEMENT_NODE || !xmlStrEqual(node->name, BAD_CAST "field"))
        return false;
    xmlChar *name = plain_prop(node, BAD_CAST "name");
    bool arch = name && xmlStrEqual(name, BAD_CAST "arch");
    xmlFree(name);
    return arch;
}
static void strip_arch(xmlNodePtr node) {
    for (; node; node = node->next)
        if (node->type == XML_ELEMENT_NODE) {
            if (arch_field(node)) {
                xmlFreeNodeList(node->children);
                node->children = node->last = NULL;
                xmlUnsetNsProp(node, NULL, BAD_CAST "eval");
                xmlUnsetNsProp(node, NULL, BAD_CAST "ref");
                xmlUnsetNsProp(node, NULL, BAD_CAST "file");
            } else
                strip_arch(node->children);
        }
}
static xmlNodePtr find_id(xmlNodePtr node, unsigned *position, unsigned ordinal) {
    for (; node; node = node->next)
        if (node->type == XML_ELEMENT_NODE) {
            xmlChar *id = plain_prop(node, BAD_CAST "id");
            bool has = id && *id;
            xmlFree(id);
            if (has && (*position)++ == ordinal)
                return node;
            xmlNodePtr found = find_id(node->children, position, ordinal);
            if (found)
                return found;
        }
    return NULL;
}
typedef struct {
    char *text;
    size_t used;
    bool ok;
} Writer;
static void putn(Writer *out, const char *s, size_t n) {
    if (!out->ok || n > XML_REF_OUTPUT_MAX - out->used) {
        out->ok = false;
        return;
    }
    memcpy(out->text + out->used, s, n);
    out->used += n;
    out->text[out->used] = 0;
}
static void put(Writer *out, const char *s) {
    if (s)
        putn(out, s, strlen(s));
}
static void escape(Writer *out, const xmlChar *value, bool attribute) {
    if (!value)
        return;
    for (const unsigned char *p = value; *p && out->ok; p++)
        switch (*p) {
        case '&':
            put(out, "&amp;");
            break;
        case '<':
            put(out, "&lt;");
            break;
        case '>':
            put(out, "&gt;");
            break;
        case '"':
            if (attribute)
                put(out, "&quot;");
            else
                putn(out, (const char *)p, 1);
            break;
        case '\t':
            if (attribute)
                put(out, "&#9;");
            else
                putn(out, (const char *)p, 1);
            break;
        case '\r':
            put(out, "&#13;");
            break;
        case '\n':
            put(out, "&#10;");
            break;
        default:
            putn(out, (const char *)p, 1);
            break;
        }
}
static void qname(Writer *out, xmlNodePtr node) {
    if (node->ns && node->ns->prefix) {
        put(out, (const char *)node->ns->prefix);
        put(out, ":");
    }
    put(out, (const char *)node->name);
}
static bool whitespace(const xmlChar *p) {
    if (!p)
        return true;
    for (; *p; p++)
        if (!strchr(" \t\r\n", *p))
            return false;
    return true;
}
static void indent(Writer *out, int depth) {
    put(out, "\n");
    for (int i = 0; i < depth; i++)
        put(out, "    ");
}
static void write_node(Writer *out, xmlNodePtr node, int depth, bool preserve, bool inlined) {
    if (!out->ok)
        return;
    if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE) {
        escape(out, node->content, false);
        return;
    }
    if (node->type == XML_COMMENT_NODE) {
        put(out, "<!--");
        put(out, (const char *)node->content);
        put(out, "-->");
        return;
    }
    if (node->type == XML_PI_NODE) {
        put(out, "<?");
        put(out, (const char *)node->name);
        if (node->content && *node->content) {
            put(out, " ");
            put(out, (const char *)node->content);
        }
        put(out, "?>");
        return;
    }
    if (node->type != XML_ELEMENT_NODE) {
        out->ok = false;
        return;
    }
    put(out, "<");
    qname(out, node);
    for (xmlNsPtr ns = node->nsDef; ns; ns = ns->next) {
        put(out, " xmlns");
        if (ns->prefix) {
            put(out, ":");
            put(out, (const char *)ns->prefix);
        }
        put(out, "=\"");
        escape(out, ns->href, true);
        put(out, "\"");
    }
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        put(out, " ");
        if (attr->ns && attr->ns->prefix) {
            put(out, (const char *)attr->ns->prefix);
            put(out, ":");
        }
        put(out, (const char *)attr->name);
        put(out, "=\"");
        xmlChar *value = xmlNodeListGetString(node->doc, attr->children, 1);
        escape(out, value, true);
        xmlFree(value);
        put(out, "\"");
    }
    if (!node->children && !arch_field(node)) {
        put(out, "/>");
        return;
    }
    put(out, ">");
    int space = xmlNodeGetSpacePreserve(node);
    if (space >= 0)
        preserve = space != 0;
    bool structure = false, mixed = false;
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE || child->type == XML_COMMENT_NODE || child->type == XML_PI_NODE)
            structure = true;
        if ((child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) &&
            !whitespace(child->content))
            mixed = true;
    }
    bool block = structure && !mixed && !preserve && !inlined;
    for (xmlNodePtr child = node->children; child && out->ok; child = child->next) {
        if (block && (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) &&
            whitespace(child->content))
            continue;
        if (block)
            indent(out, depth + 1);
        write_node(out, child, depth + 1, preserve, !block);
    }
    if (block)
        indent(out, depth);
    put(out, "</");
    qname(out, node);
    put(out, ">");
}
char *xml_refs_snippet(const XmlReferences *refs, int index, char *error, size_t cap) {
    if (!refs || index < 0 || index >= refs->count || !refs->raw) {
        fail(error, cap, "Identifiant XML invalide.");
        return NULL;
    }
    xmlDocPtr doc = xmlReadMemory(refs->raw, (int)refs->len, NULL, "UTF-8",
                                  XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
                                      XML_PARSE_COMPACT | XML_PARSE_BIG_LINES | XML_PARSE_IGNORE_ENC);
    if (!doc) {
        fail(error, cap, "XML invalide ou mémoire insuffisante.");
        return NULL;
    }
    unsigned position = 0;
    xmlNodePtr source = find_id(xmlDocGetRootElement(doc), &position, refs->items[index].ordinal);
    xmlDocPtr copy = source ? xmlNewDoc(BAD_CAST "1.0") : NULL;
    xmlNodePtr root = copy ? xmlDocCopyNode(source, copy, 1) : NULL;
    if (!root) {
        if (copy)
            xmlFreeDoc(copy);
        xmlFreeDoc(doc);
        fail(error, cap, "Mémoire insuffisante ou identifiant XML absent.");
        return NULL;
    }
    xmlDocSetRootElement(copy, root);
    bool ok = true;
    /* Inherited namespaces (even those used only in attribute text) stay available. */
    for (xmlNodePtr parent = source->parent; parent && parent->type == XML_ELEMENT_NODE;
         parent = parent->parent)
        for (xmlNsPtr ns = parent->nsDef; ns; ns = ns->next)
            if (!xmlSearchNs(copy, root, ns->prefix) && !xmlNewNs(root, ns->href, ns->prefix))
                ok = false;
    if (xmlReconciliateNs(copy, root) < 0)
        ok = false;
    int space = xmlNodeGetSpacePreserve(source);
    if (space >= 0 && xmlNodeGetSpacePreserve(root) < 0)
        xmlNodeSetSpacePreserve(root, space);
    strip_arch(root);
    Writer out = {malloc(XML_REF_OUTPUT_MAX + 1), 0, ok};
    if (!out.text)
        out.ok = false;
    if (out.ok) {
        out.text[0] = 0;
        write_node(&out, root, 0, false, false);
    }
    xmlFreeDoc(copy);
    xmlFreeDoc(doc);
    if (!out.ok) {
        free(out.text);
        fail(error, cap, "Bloc XML trop volumineux (256 Kio maximum) ou mémoire insuffisante.");
        return NULL;
    }
    return out.text;
}
static void load(XmlLoadJob *job) {
    for (int i = 0; i < job->path_count && !atomic_load(&job->cancel); i++) {
        const char *path = job->paths[i];
        if (job->guard && !job->guard(job->guard_context, path, false, job->error, sizeof(job->error)))
            return;
#ifdef _WIN32
        int fd = _open(path, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
        if (fd < 0) {
            if (errno == ENOENT || errno == ENOTDIR)
                continue;
            snprintf(job->error, sizeof(job->error), "Lecture XML impossible : %s", strerror(errno));
            return;
        }
        struct stat st;
        if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
            (unsigned long long)st.st_size > XML_REF_SOURCE_MAX) {
            close(fd);
            fail(job->error, sizeof(job->error), "Source XML : fichier régulier de 8 Mio maximum requis.");
            return;
        }
        char *raw = malloc(XML_REF_SOURCE_MAX + 1);
        if (!raw) {
            close(fd);
            fail(job->error, sizeof(job->error), "Mémoire insuffisante.");
            return;
        }
        size_t used = 0;
        bool ok = true;
        while (!atomic_load(&job->cancel)) {
            size_t size = XML_REF_SOURCE_MAX + 1 - used;
            if (size > 65536)
                size = 65536;
            long count = (long)read(fd, raw + used, (unsigned)size);
            if (count < 0) {
                if (errno == EINTR)
                    continue;
                ok = fail(job->error, sizeof(job->error), "Erreur de lecture du fichier XML.");
                break;
            }
            if (!count)
                break;
            used += (size_t)count;
            if (used > XML_REF_SOURCE_MAX) {
                ok = fail(job->error, sizeof(job->error), "Fichier XML trop volumineux (8 Mio maximum).");
                break;
            }
        }
        close(fd);
        if (atomic_load(&job->cancel))
            ok = false;
        if (ok && job->guard)
            ok = job->guard(job->guard_context, path, false, job->error, sizeof(job->error));
        if (ok)
            ok = xml_refs_parse(&job->references, raw, used, &job->cancel, job->error, sizeof(job->error));
        free(raw);
        if (ok) {
            strcpy(job->references.path, path);
            job->success = true;
        }
        return;
    }
    if (!atomic_load(&job->cancel))
        fail(job->error, sizeof(job->error),
             "Fichier XML introuvable dans les dossiers XML, le projet, le dossier du document ou les "
             "imports configurés.");
}
#ifdef _WIN32
static DWORD WINAPI work(void *arg)
#else
static void *work(void *arg)
#endif
{
    XmlLoadJob *job = arg;
    load(job);
    atomic_store(&job->done, true);
    return 0;
}
XmlLoadJob *xml_job_start(const char paths[][BUF_PATH], int count, FsGuard guard, void *context, char *error,
                          size_t cap) {
    if (count < 1 || count > XML_REF_ROOTS_MAX) {
        fail(error, cap, "Chemin XML invalide.");
        return NULL;
    }
    /* libxml2 2.9 must be initialized on the caller/UI thread before workers. */
    xmlInitParser();
    XmlLoadJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fail(error, cap, "Mémoire insuffisante.");
        return NULL;
    }
    atomic_init(&job->cancel, false);
    atomic_init(&job->done, false);
    job->path_count = count;
    job->guard = guard;
    job->guard_context = context;
    for (int i = 0; i < count; i++) {
        size_t len = 0;
        while (len < BUF_PATH && paths[i][len])
            len++;
        if (!len || len == BUF_PATH) {
            free(job);
            fail(error, cap, "Chemin XML invalide.");
            return NULL;
        }
        strcpy(job->paths[i], paths[i]);
    }
#ifdef _WIN32
    job->thread = CreateThread(NULL, 0, work, job, 0, NULL);
    bool ok = job->thread != NULL;
#else
    pthread_t *thread = malloc(sizeof(*thread));
    job->thread = thread;
    bool ok = thread && pthread_create(thread, NULL, work, job) == 0;
#endif
    if (!ok) {
#ifndef _WIN32
        free(job->thread);
#endif
        free(job);
        fail(error, cap, "Impossible de démarrer la lecture XML.");
        return NULL;
    }
    return job;
}
void xml_job_cancel(XmlLoadJob *job) {
    if (job)
        atomic_store(&job->cancel, true);
}
void xml_job_free(XmlLoadJob *job) {
    if (!job)
        return;
    xml_job_cancel(job);
#ifdef _WIN32
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
#else
    pthread_join(*(pthread_t *)job->thread, NULL);
    free(job->thread);
#endif
    xml_refs_free(&job->references);
    free(job);
}
