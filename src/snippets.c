#include "snippets.h"
#include "files.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNIPPETS_FILE_MAX ((SNIPPET_BODY_MAX + SNIPPET_TRIGGER + SNIPPET_TITLE + 64u) * SNIPPETS_MAX + 64u)

static bool fail(char *error, size_t cap, const char *message) {
    if (cap)
        snprintf(error, cap, "%s", message);
    return false;
}
static bool text_valid(const char *text, size_t limit, bool multiline) {
    if (!text)
        return false;
    size_t n = 0;
    while (n <= limit && text[n]) {
        unsigned char c = (unsigned char)text[n];
        if ((c < 32 && !(multiline && (c == '\n' || c == '\r' || c == '\t'))) || c == 127)
            return false;
        n++;
    }
    return n <= limit && utf8_valid(text, n);
}
static bool trigger_valid(const char *trigger) {
    if (!trigger || !*trigger || strlen(trigger) >= SNIPPET_TRIGGER)
        return false;
    for (size_t i = 0; trigger[i]; i++) {
        unsigned char c = (unsigned char)trigger[i];
        if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i && c >= '0' && c <= '9')))
            return false;
    }
    return true;
}
void snippets_free(Snippets *library) {
    if (!library)
        return;
    for (int i = 0; i < library->count && i < SNIPPETS_MAX; i++)
        free(library->items[i].body);
    memset(library, 0, sizeof(*library));
}
bool snippets_valid(const Snippets *library) {
    if (!library || library->count < 0 || library->count > SNIPPETS_MAX)
        return false;
    for (int i = 0; i < library->count; i++) {
        const Snippet *s = &library->items[i];
        if (!trigger_valid(s->trigger) || !text_valid(s->title, SNIPPET_TITLE - 1, false) ||
            !text_valid(s->body, SNIPPET_BODY_MAX, true) || !*s->body)
            return false;
        for (int j = 0; j < i; j++)
            if (!strcmp(s->trigger, library->items[j].trigger))
                return false;
    }
    return true;
}
bool snippets_put(Snippets *library, int index, const char *trigger, const char *title, const char *body,
                  char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!snippets_valid(library) || index < -1 || index >= library->count)
        return fail(error, cap, "Extrait ou index invalide.");
    if (!trigger_valid(trigger))
        return fail(error, cap, "Déclencheur : lettre ou _, puis lettres/chiffres/_ (63 caractères max.).");
    if (!text_valid(title, SNIPPET_TITLE - 1, false))
        return fail(error, cap, "Titre UTF-8 sur une ligne, 95 octets maximum.");
    if (!text_valid(body, SNIPPET_BODY_MAX, true) || !*body)
        return fail(error, cap, "Code UTF-8 requis, non vide, limité à 16 Ko par extrait.");
    for (int i = 0; i < library->count; i++)
        if (i != index && !strcmp(library->items[i].trigger, trigger))
            return fail(error, cap, "Ce déclencheur existe déjà. Modifiez l’extrait correspondant.");
    if (index == -1 && library->count == SNIPPETS_MAX)
        return fail(error, cap, "Limite : 64 extraits dans la bibliothèque commune.");
    char *copy = malloc(strlen(body) + 1);
    if (!copy)
        return fail(error, cap, "Mémoire insuffisante pour l’extrait.");
    strcpy(copy, body);
    char key[SNIPPET_TRIGGER], label[SNIPPET_TITLE];
    memcpy(key, trigger, strlen(trigger) + 1);
    memcpy(label, title, strlen(title) + 1);
    if (index == -1)
        index = library->count++;
    Snippet *s = &library->items[index];
    free(s->body);
    s->body = copy;
    memcpy(s->trigger, key, strlen(key) + 1);
    memcpy(s->title, label, strlen(label) + 1);
    return true;
}
bool snippets_clone(Snippets *out, const Snippets *library) {
    if (!out || !snippets_valid(library))
        return false;
    Snippets next = {0};
    for (int i = 0; i < library->count; i++) {
        const Snippet *s = &library->items[i];
        if (!snippets_put(&next, -1, s->trigger, s->title, s->body, NULL, 0)) {
            snippets_free(&next);
            return false;
        }
    }
    snippets_free(out);
    *out = next;
    return true;
}
bool snippets_remove(Snippets *library, int index) {
    if (!snippets_valid(library) || index < 0 || index >= library->count)
        return false;
    free(library->items[index].body);
    memmove(library->items + index, library->items + index + 1,
            (size_t)(library->count - index - 1) * sizeof(Snippet));
    memset(&library->items[--library->count], 0, sizeof(Snippet));
    return true;
}
bool snippets_default(Snippets *library) {
    return snippets_put(library, -1, "xml_form", "Odoo · vue formulaire (exemple à adapter)",
                        "<odoo>\n"
                        "    <record id=\"view_mon_modele_form\" model=\"ir.ui.view\">\n"
                        "        <field name=\"name\">mon.modele.form</field>\n"
                        "        <field name=\"model\">mon.modele</field>\n"
                        "        <field name=\"arch\" type=\"xml\">\n"
                        "            <form string=\"Mon formulaire\">\n"
                        "                <sheet>\n"
                        "                    <group>\n"
                        "                        <field name=\"name\"/>\n"
                        "                    </group>\n"
                        "                </sheet>\n"
                        "            </form>\n"
                        "        </field>\n"
                        "    </record>\n"
                        "</odoo>",
                        NULL, 0);
}
bool snippets_file(char *out, size_t cap, const char *settings) {
    char parent[BUF_PATH];
    if (!out || !cap || !settings || !fs_parent(settings, parent))
        return false;
    int n = snprintf(out, cap, "%s/snippets.cfg", parent);
    return n >= 0 && (size_t)n < cap;
}
static bool decimal(char **p, size_t *n, char separator) {
    if (**p < '0' || **p > '9')
        return false;
    size_t value = 0;
    do {
        value = value * 10 + (size_t)(*(*p)++ - '0');
        if (value > SNIPPETS_FILE_MAX)
            return false;
    } while (**p >= '0' && **p <= '9');
    if (*(*p)++ != separator)
        return false;
    *n = value;
    return true;
}
bool snippets_load(Snippets *out, const char *file, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !file)
        return fail(error, cap, "Fichier d’extraits invalide.");
    FILE *f = fopen(file, "rb");
    if (!f)
        return errno == ENOENT ? false : fail(error, cap, strerror(errno));
    char *text = malloc(SNIPPETS_FILE_MAX + 1u);
    Snippets next = {0};
    if (!text) {
        fclose(f);
        return fail(error, cap, "Mémoire insuffisante pour les extraits.");
    }
    size_t n = fread(text, 1, SNIPPETS_FILE_MAX + 1u, f);
    bool ok = !ferror(f) && n <= SNIPPETS_FILE_MAX && !memchr(text, 0, n);
    fclose(f);
    if (ok)
        text[n] = 0;
    const char header[] = "PRISME-SNIPPETS\t1\n";
    char *p = text, *end = text + (ok ? n : 0);
    ok = ok && n >= sizeof(header) - 1 && !memcmp(p, header, sizeof(header) - 1);
    if (ok)
        p += sizeof(header) - 1;
    bool ended = false;
    while (ok && p < end) {
        if (!strcmp(p, "END\n")) {
            ended = true;
            break;
        }
        char *line = memchr(p, '\n', (size_t)(end - p));
        if (!line || line - p > 128 || strncmp(p, "SNIP\t", 5)) {
            ok = false;
            break;
        }
        *line = 0;
        char *key = p + 5, *tab = strchr(key, '\t');
        size_t title_n = 0, body_n = 0;
        ok = tab != NULL;
        if (!ok)
            break;
        *tab = 0;
        p = tab + 1;
        ok = decimal(&p, &title_n, '\t') && decimal(&p, &body_n, 0) && title_n < SNIPPET_TITLE &&
             body_n <= SNIPPET_BODY_MAX && (size_t)(end - line - 1) > title_n + body_n;
        if (!ok)
            break;
        char title[SNIPPET_TITLE];
        memcpy(title, line + 1, title_n);
        title[title_n] = 0;
        char *body = line + 1 + title_n, *following = body + body_n;
        ok = *following == '\n';
        *following = 0;
        if (ok)
            ok = snippets_put(&next, -1, key, title, body, error, cap);
        p = following + 1;
    }
    ok = ok && ended;
    free(text);
    if (!ok) {
        snippets_free(&next);
        return fail(error, cap, "Bibliothèque d’extraits invalide/tronquée ; aucun extrait remplacé.");
    }
    snippets_free(out);
    *out = next;
    return true;
}
bool snippets_save(const Snippets *library, const char *file, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!file || !snippets_valid(library))
        return fail(error, cap, "Bibliothèque d’extraits invalide.");
    char *text = malloc(SNIPPETS_FILE_MAX + 1u);
    Buffer *buffer = calloc(1, sizeof(*buffer));
    if (!text || !buffer) {
        free(text);
        free(buffer);
        return fail(error, cap, "Mémoire insuffisante pour enregistrer les extraits.");
    }
    size_t used = (size_t)sprintf(text, "PRISME-SNIPPETS\t1\n");
    for (int i = 0; i < library->count; i++) {
        const Snippet *s = &library->items[i];
        size_t a = strlen(s->title), b = strlen(s->body);
        used += (size_t)sprintf(text + used, "SNIP\t%s\t%zu\t%zu\n", s->trigger, a, b);
        memcpy(text + used, s->title, a);
        used += a;
        memcpy(text + used, s->body, b);
        used += b;
        text[used++] = '\n';
    }
    memcpy(text + used, "END\n", 5);
    bool ok = buf_init(buffer, text, "snippets.cfg");
    if (ok)
        ok = buf_save(buffer, file, error, cap);
    else
        fail(error, cap, "Mémoire insuffisante pour la bibliothèque.");
    buf_free(buffer);
    free(buffer);
    free(text);
    return ok;
}
int snippets_match(const Snippets *library, const char *prefix, int *indices, int capacity) {
    if (!library || library->count < 0 || library->count > SNIPPETS_MAX || !prefix || !indices ||
        capacity <= 0)
        return 0;
    size_t n = strlen(prefix);
    int count = 0;
    for (int i = 0; i < library->count; i++)
        if (!strncmp(library->items[i].trigger, prefix, n)) {
            int pos = 0;
            while (pos < count) {
                const char *a = library->items[indices[pos]].trigger, *b = library->items[i].trigger;
                if (strlen(a) > strlen(b) || (strlen(a) == strlen(b) && strcmp(a, b) > 0))
                    break;
                pos++;
            }
            if (pos < capacity) {
                if (count < capacity)
                    count++;
                memmove(indices + pos + 1, indices + pos, (size_t)(count - pos - 1) * sizeof(int));
                indices[pos] = i;
            }
        }
    return count;
}
bool snippet_expand(Buffer *document, const Snippet *snippet, size_t start, size_t end, char *error,
                    size_t cap) {
    if (cap)
        error[0] = 0;
    if (!document || !snippet || !text_valid(snippet->body, SNIPPET_BODY_MAX, true) || start > end ||
        end > document->len || document->extra_count ||
        (start < document->len && ((unsigned char)document->text[start] & 0xc0) == 0x80) ||
        (end < document->len && ((unsigned char)document->text[end] & 0xc0) == 0x80))
        return fail(error, cap, "Insertion invalide ; utilisez un seul curseur pour les extraits.");
    size_t line = start;
    while (line && document->text[line - 1] != '\n')
        line--;
    size_t indent = 0;
    while (line + indent < start &&
           (document->text[line + indent] == ' ' || document->text[line + indent] == '\t'))
        indent++;
    size_t n = strlen(snippet->body), lines = 0;
    for (size_t i = 0; i < n; i++)
        if (snippet->body[i] == '\n' || (snippet->body[i] == '\r' && snippet->body[i + 1] != '\n'))
            lines++;
    if (n > BUF_LIMIT || (indent && lines > (BUF_LIMIT - n) / indent))
        return fail(error, cap, "Extrait trop volumineux après indentation.");
    size_t size = n + indent * lines;
    if (document->len - (end - start) > BUF_LIMIT - size)
        return fail(error, cap, "Insertion impossible : limite de 2 Mo par document.");
    char *expanded = malloc(size + 1);
    if (!expanded)
        return fail(error, cap, "Mémoire insuffisante pour insérer l’extrait.");
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        char c = snippet->body[i];
        if (c == '\r') {
            if (snippet->body[i + 1] == '\n')
                i++;
            c = '\n';
        }
        expanded[used++] = c;
        if (c == '\n' && i + 1 < n) {
            memcpy(expanded + used, document->text + line, indent);
            used += indent;
        }
    }
    expanded[used] = 0;
    bool ok = buf_replace(document, start, end, expanded);
    free(expanded);
    return ok || fail(error, cap, "Insertion impossible : mémoire ou texte UTF-8 invalide.");
}
