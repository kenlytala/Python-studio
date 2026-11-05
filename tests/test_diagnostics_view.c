/* Actual view.c functions, recorded calls only. No window, GL, image or preview. */
#include "app_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BOX(x, y, w, h, t) ui_box((float)(x), (float)(y), (float)(w), (float)(h), t, 0, 255)
#define RECT(x, y, w, h) ((Rectangle){(float)(x), (float)(y), (float)(w), (float)(h)})
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
const Color ui_colors[C_COUNT] = {{0}};
static App *current;
static int phase, scissors, status_bottom, buttons, opened_icon, segments;
static Tone last_wave;
static const char *forbidden_secret;
enum { NORMAL, TOPBAR, SIDEBAR, UNDERLINE, STATUS, CONTEXT, FOLDER };
static int folder_highlights, folder_active, folder_clip_count;
static Rectangle folder_clip;
static bool folder_tail_painted;
static float menu_text_x[5], menu_icon_x[5];
static int menu_text_count, menu_icon_count;
Vector2 app_caret(App *a, size_t p) {
    return (Vector2){(float)a->code_x + (float)buf_column(app_buffer(a), p) * a->char_w,
                     (float)a->code_y + 13 + (float)buf_row(app_buffer(a), p) * a->line_h};
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
LintReport *app_diagnostics(App *a) {
    LintDocument *d = a->analyses[a->active];
    if (!d || !d->valid || d->analyzed_version != app_buffer(a)->version)
        return NULL;
    lint_resolve(&d->report, app_buffer(a));
    return &d->report;
}
static void caption(const char *s) {
    CHECK(s && utf8_valid(s, strlen(s)));
    if (forbidden_secret)
        CHECK(!strstr(s, forbidden_secret));
    if (phase == SIDEBAR)
        CHECK(!strstr(s, "Moins de bruit") && !strstr(s, "MOTEUR NATIF") && !strstr(s, "100 % natif"));
}
float ui_text_size(float size) {
    return fmaxf(17, size);
}
float ui_width(UI *u, const char *s, float size, bool mono) {
    (void)u;
    (void)size;
    return (float)strlen(s) * (mono ? 10.2f : 8.3f);
}
Color ui_alpha(Tone t, int alpha) {
    Color c = ui_colors[t];
    c.a = (unsigned char)alpha;
    return c;
}
bool ui_inside(Rectangle r, Vector2 p) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
void ui_box(float x, float y, float w, float h, Tone t, float radius, int alpha) {
    (void)radius;
    (void)alpha;
    CHECK(w >= 0 && h >= 0);
    if (phase == FOLDER && t == C_NUMBER && alpha == 42)
        folder_highlights++;
    if (phase == FOLDER && t == C_ACCENT && alpha == 70)
        folder_active++;
    if (phase == STATUS && t == C_STATUS) {
        CHECK(x == 0 && w == current->w);
        status_bottom = (int)(y + h);
    }
}
void ui_outline(float x, float y, float w, float h, Tone t, float radius, int alpha) {
    (void)x;
    (void)y;
    (void)t;
    (void)radius;
    (void)alpha;
    CHECK(w >= 0 && h >= 0);
}
void ui_line(float x, float y, float xx, float yy, Tone t, float thick, int alpha) {
    (void)thick;
    (void)alpha;
    if (phase == UNDERLINE) {
        CHECK(x >= current->code_x && xx <= current->code_right);
        CHECK(y >= current->code_y && yy <= current->code_bottom);
        last_wave = t;
        segments++;
    }
}
void ui_text(UI *u, const char *s, float x, float y, float size, Tone t, bool mono) {
    (void)u;
    (void)x;
    (void)y;
    (void)size;
    (void)t;
    (void)mono;
    caption(s);
    if (phase == FOLDER && strstr(s, "tailneedle") && x >= current->folder_search.content_rect.x + 66)
        folder_tail_painted = true;
}
void ui_text_fit(UI *u, const char *s, float x, float y, float w, float size, Tone t, bool mono) {
    (void)u;
    (void)y;
    (void)size;
    (void)t;
    (void)mono;
    CHECK(w >= 0);
    caption(s);
    if (phase == CONTEXT && (!strcmp(s, "Créer un fichier…") || !strcmp(s, "Créer un dossier…") ||
                             !strcmp(s, "Copier") || !strcmp(s, "Coller ici") || !strcmp(s, "Supprimer…"))) {
        CHECK(menu_text_count < 5);
        menu_text_x[menu_text_count++] = x;
    }
    if (phase == STATUS) {
        CHECK(x >= 502 && x + w <= current->w - 18);
    }
}
void ui_label(UI *u, const char *s, float x, float y, float size, Tone t) {
    (void)u;
    (void)x;
    (void)y;
    (void)size;
    (void)t;
    caption(s);
}
void ui_icon(UI *u, Icon i, float x, float y, float size, Tone t, int alpha) {
    (void)u;
    (void)i;
    (void)x;
    (void)y;
    (void)size;
    (void)t;
    (void)alpha;
    if (phase == CONTEXT) {
        CHECK(menu_icon_count < 5);
        menu_icon_x[menu_icon_count++] = x;
    }
}
bool ui_hit(UI *u, Rectangle r, int action, int arg, const char *tip) {
    CHECK(u->hit_count < 512);
    u->hits[u->hit_count++] = (Hit){r, action, arg, u->layer, tip};
    return ui_inside(r, u->mouse);
}
void ui_button(UI *u, Rectangle r, int action, int arg, const char *s, int icon, int style, bool active,
               const char *tip) {
    (void)style;
    (void)active;
    buttons++;
    if (s)
        caption(s);
    ui_hit(u, r, action, arg, tip);
    if (action == A_MINIMIZE) {
        CHECK(icon == I_MINUS);
        CHECK(tip && strstr(tip, "Réduire") && strstr(tip, "sans quitter"));
    }
    if (action == A_LINT_IGNORE) {
        char code[6];
        CHECK(icon == I_MINUS && lint_rule_decode(arg, code));
        CHECK(tip && strstr(tip, code) &&
              strstr(tip, odoo_diagnostic_code(code) ? "projet Odoo" : "tous les projets"));
    }
    if (phase == SIDEBAR && action == A_OPEN) {
        CHECK(tip && !strcmp(tip, "Ouvrir un fichier · Ctrl+O"));
        opened_icon = icon;
    }
}
void BeginScissorMode(int x, int y, int w, int h) {
    (void)x;
    (void)y;
    CHECK(w >= 0 && h >= 0);
    Rectangle right = current->folder_search.content_rect;
    if (phase == FOLDER && x == (int)(right.x + 2) && y == (int)(right.y + 36)) {
        folder_clip = (Rectangle){(float)x, (float)y, (float)w, (float)h};
        folder_clip_count++;
    }
    scissors++;
}
void EndScissorMode(void) {
    CHECK(scissors > 0);
    scissors--;
}
bool IsWindowFullscreen(void) {
    return false;
}
void DrawCircleGradient(int x, int y, float r, Color a, Color b) {
    (void)x;
    (void)y;
    (void)r;
    (void)a;
    (void)b;
}
void DrawCircle(int x, int y, float radius, Color c) {
    (void)x;
    (void)y;
    (void)radius;
    (void)c;
}
double GetTime(void) {
    return 0;
}
int GetFPS(void) {
    return 60;
}
bool IsFileExtension(const char *s, const char *end) {
    size_t n = strlen(s), m = strlen(end);
    return n >= m && !strcmp(s + n - m, end);
}
const char *GetFileExtension(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}
Action app_context_action(int index) {
    const Action actions[] = {A_FS_NEW_FILE, A_FS_NEW_FOLDER, A_FS_COPY, A_FS_PASTE, A_FS_DELETE};
    return index >= 0 && index < 5 ? actions[index] : A_NONE;
}
bool app_context_enabled(App *a, int index) {
    (void)a;
    return index >= 0 && index < 5;
}
void app_field_draw(App *a, Buffer *b, Rectangle r, bool focus, const char *p, Action id, bool subtle) {
    (void)a;
    caption(b->text);
    (void)r;
    (void)focus;
    (void)p;
    (void)id;
    (void)subtle;
}
#include "prisme_view_functions.inc"
static Hit *find(UI *u, int action) {
    for (int i = 0; i < u->hit_count; i++)
        if (u->hits[i].action == action)
            return &u->hits[i];
    return NULL;
}
static void reset(App *a, int value) {
    a->ui.hit_count = 0;
    a->ui.layer = 0;
    buttons = 0;
    phase = value;
    folder_clip_count = 0;
    folder_clip = (Rectangle){0};
}
static void search_bars_bounds(App *a) {
    FolderSearchState *s = &a->folder_search;
    Rectangle h = s->htrack, v = s->vtrack, ht = s->hthumb, vt = s->vthumb, r = s->content_rect;
    CHECK(h.width > 0 && h.height >= 12 && v.width >= 12 && v.height > 0);
    CHECK(h.x >= r.x + 66 && h.x + h.width <= v.x + .001f && v.y + v.height <= h.y + .001f);
    CHECK(h.y + h.height <= r.y + r.height && v.x + v.width <= r.x + r.width);
    CHECK(ht.x >= h.x - .001f && ht.x + ht.width <= h.x + h.width + .001f && ht.y == h.y &&
          ht.height == h.height);
    CHECK(vt.y >= v.y - .001f && vt.y + vt.height <= v.y + v.height + .001f && vt.x == v.x &&
          vt.width == v.width);
    CHECK(folder_clip_count == 1 && folder_clip.x + folder_clip.width <= v.x &&
          folder_clip.y + folder_clip.height <= h.y);
    CHECK(s->horizontal.limit == (s->extent_columns > s->columns ? s->extent_columns - s->columns : 0));
    unsigned bars = 0;
    for (int i = 0; i < a->ui.hit_count; i++) {
        Hit *hit = &a->ui.hits[i];
        Rectangle q = hit->rect;
        if (hit->action != A_FOLDER_SEARCH_FOCUS || hit->arg != 3)
            continue;
        if ((q.x == h.x && q.y == h.y && q.width == h.width && q.height == h.height) ||
            (q.x == v.x && q.y == v.y && q.width == v.width && q.height == v.height))
            bars++;
    }
    CHECK(bars == 2);
}
static void folder_search_views(App *a) {
    FolderSearchState *s = &a->folder_search;
    SearchReport report = {0};
    SearchFile files[2] = {0};
    char error[512];
    strcpy(report.root, "/project");
    strcpy(report.query, "needle");
    report.files = files;
    report.count = 2;
    report.hit_count = 3;
    report.examined = 2;
    strcpy(files[0].path, "/project/a.txt");
    strcpy(files[0].relative, "a.txt");
    files[0].text = "needle + needle\n";
    files[0].len = strlen(files[0].text);
    strcpy(files[1].path, "/project/sub/b.txt");
    strcpy(files[1].relative, "sub/b.txt");
    files[1].text = "α\tneedle\n";
    files[1].len = strlen(files[1].text);
    for (unsigned i = 0; i < 2; i++)
        CHECK(folder_search_find(files[i].text, files[i].len, "needle", SEARCH_HITS_MAX, &files[i].hits,
                                 &files[i].count, error, sizeof(error)));
    CHECK(buf_init(&s->folder, "/project", "folder") && buf_init(&s->query, "needle", "query") &&
          buf_init(&s->view, files[0].text, "snapshot"));
    buf_index(&s->view);
    s->initialized = true;
    s->report = &report;
    s->file = s->hit = 0;
    s->focus = 2;
    int widths[] = {960, 1440, 1920}, heights[] = {600, 900};
    for (unsigned wi = 0; wi < 3; wi++)
        for (unsigned hi = 0; hi < 2; hi++) {
            a->w = widths[wi];
            a->h = heights[hi];
            a->font_size = 20;
            a->modal = M_FOLDER_SEARCH;
            reset(a, FOLDER);
            folder_highlights = folder_active = 0;
            app_folder_search_draw(a);
            search_bars_bounds(a);
            CHECK(!scissors && folder_highlights == 1 && folder_active == 1);
            CHECK(s->list_rect.x >= 16 && s->list_rect.x + s->list_rect.width < s->content_rect.x);
            CHECK(s->content_rect.x + s->content_rect.width <= a->w - 16 && s->rows > 0 && s->columns > 0);
            CHECK(s->list_rect.y + s->list_rect.height < a->h - 60);
            const Hit *next = find(&a->ui, A_FOLDER_SEARCH_NEXT), *prev = find(&a->ui, A_FOLDER_SEARCH_PREV),
                      *open = find(&a->ui, A_FOLDER_SEARCH_OPEN), *file = find(&a->ui, A_FOLDER_SEARCH_FILE);
            CHECK(next && prev && open && file && find(&a->ui, A_FOLDER_SEARCH_START) &&
                  find(&a->ui, A_FOLDER_SEARCH_PICK_ROOT));
            CHECK(next->rect.x >= 0 && open->rect.x + open->rect.width <= a->w &&
                  file->rect.x < s->content_rect.x);
        }
    for (unsigned i = 0; i < 4; i++)
        CHECK(buf_init(&s->filters[i], i == 0 ? "*.txt" : "", "filter"));
    s->options = report.options = folder_search_options();
    for (unsigned maximized = 0; maximized < 2; maximized++)
        for (unsigned filters = 0; filters < 2; filters++)
            for (unsigned wi = 0; wi < 3; wi++)
                for (unsigned hi = 0; hi < 2; hi++) {
                    a->w = widths[wi];
                    a->h = heights[hi];
                    s->maximized = maximized != 0;
                    s->filters_open = filters != 0;
                    reset(a, FOLDER);
                    app_folder_search_draw(a);
                    search_bars_bounds(a);
                    CHECK(find(&a->ui, A_FOLDER_SEARCH_MAXIMIZE) &&
                          !find(&a->ui, A_FOLDER_SEARCH_FULLSCREEN) && find(&a->ui, A_FOLDER_SEARCH_OPTION) &&
                          find(&a->ui, A_FOLDER_SEARCH_FILTERS));
                    CHECK(s->shell_rect.x >= 0 && s->shell_rect.width <= a->w &&
                          s->list_rect.y + s->list_rect.height < a->h - 60);
                    CHECK(s->content_rect.x > s->list_rect.x + s->list_rect.width &&
                          s->content_rect.width > 0 && s->rows > 0);
                    if (maximized)
                        CHECK(s->shell_rect.x == 0 && s->shell_rect.y == 0 && s->shell_rect.width == a->w &&
                              s->shell_rect.height == a->h);
                }
    /* A match continuing from an earlier line and an empty match have a visible
       highlight, even when the first line is above the scissor viewport. */
    SearchFile multi = {0};
    multi.text = "needle\nα\ty\nend\n"
                 "line\nline\nline\nline\nline\nline\nline\nline\nline\nline\n"
                 "line\nline\nline\nline\nline\nline\nline\nline\nline\nline\n"
                 "line\nline\nline\nline\nline\nline\nline\nline\nline\nline\n";
    multi.len = strlen(multi.text);
    SearchOptions regex = folder_search_options();
    regex.regex = true;
    CHECK(folder_search_find_options(multi.text, multi.len, "(?s)needle.*y|(?=end)", SEARCH_HITS_MAX, &regex,
                                     &multi.hits, &multi.count, error, sizeof(error)) &&
          multi.count == 2);
    SearchFile *original = report.files;
    report.files = &multi;
    report.count = 1;
    s->file = s->hit = 0;
    s->extent_columns = 0;
    s->y = 1;
    s->x = 0;
    s->maximized = true;
    s->filters_open = false;
    buf_free(&s->view);
    CHECK(buf_init(&s->view, multi.text, "multiline"));
    buf_index(&s->view);
    a->w = 1440;
    a->h = 900;
    reset(a, FOLDER);
    folder_highlights = folder_active = 0;
    app_folder_search_draw(a);
    CHECK(folder_active >= 1 && folder_highlights >= 1 && !scissors);
    free(multi.hits);
    /* Read-only tail of a 200k-cell line below the initial viewport: actual
       clipping/drawing must reach it, with independent horizontal/vertical bars. */
    SearchFile wide = {0};
    wide.text = calloc(220000, 1);
    CHECK(wide.text);
    size_t p = 0;
    memcpy(wide.text + p, "needle\n", 7);
    p += 7;
    for (int i = 0; i < 80; i++) {
        memcpy(wide.text + p, "row\n", 4);
        p += 4;
    }
    memcpy(wide.text + p, "α\t", 3);
    p += 3;
    memset(wide.text + p, 'a', 200000);
    p += 200000;
    strcpy(wide.text + p, "\t猫😀 tailneedle\n");
    wide.len = strlen(wide.text);
    strcpy(wide.path, "/project/wide.txt");
    strcpy(wide.relative, "wide.txt");
    CHECK(folder_search_find(wide.text, wide.len, "needle", SEARCH_HITS_MAX, &wide.hits, &wide.count, error,
                             sizeof(error)) &&
          wide.count == 2);
    report.files = &wide;
    report.count = 1;
    s->file = 0;
    s->hit = 1;
    s->extent_columns = 0;
    s->x = s->y = (unsigned)-1;
    s->maximized = false;
    s->filters_open = false;
    a->w = 960;
    a->h = 640;
    buf_free(&s->view);
    CHECK(buf_init(&s->view, wide.text, "snapshot"));
    buf_index(&s->view);
    folder_tail_painted = false;
    folder_highlights = folder_active = 0;
    reset(a, FOLDER);
    app_folder_search_draw(a);
    search_bars_bounds(a);
    CHECK(s->extent_columns == 200022 && s->x == s->horizontal.limit && s->y == s->vertical.limit);
    CHECK(folder_tail_painted && folder_active == 1 && folder_highlights == 0 && !scissors);
    CHECK(s->horizontal.limit > 199900 && s->vertical.limit > 70);
    /* A layout change drops stale pixel grab offsets before painting new tracks. */
    s->scroll_drag = 1;
    s->scroll_grab = 3;
    a->w = 1440;
    s->maximized = true;
    reset(a, FOLDER);
    app_folder_search_draw(a);
    search_bars_bounds(a);
    CHECK(!s->scroll_drag && s->x <= s->horizontal.limit);
    s->scroll_drag = 2;
    s->filters_open = true;
    reset(a, FOLDER);
    app_folder_search_draw(a);
    search_bars_bounds(a);
    CHECK(!s->scroll_drag && s->y <= s->vertical.limit);
    free(wide.hits);
    free(wide.text);
    report.files = original;
    report.count = 2;
    for (unsigned i = 0; i < 4; i++)
        buf_free(&s->filters[i]);
    puts("OK 24 extra native search layouts: all option/filter/maximize combinations, UTF-8 multiline "
         "continuation/zero-width marker; 200k-cell Unicode/tab line tail painted, both bar bands "
         "clipped/hit-tested, resize/filters cancel drag; no window");
    Buffer text = {0};
    CHECK(buf_init(&text, "α\tété\t猫😀", "cells"));
    char visible[64];
    CHECK(app_folder_search_line(&text, 0, 2, 5, visible, sizeof(visible)) == 7 && !strcmp(visible, "  été"));
    CHECK(app_folder_search_line(&text, 0, 5, 5, visible, sizeof(visible)) == 11 &&
          !strcmp(visible, "té 猫😀"));
    CHECK(app_folder_search_line(&text, 0, 8, 2, visible, 4) == 3 && !strcmp(visible, "猫") &&
          utf8_valid(visible, strlen(visible)));
    CHECK(!app_folder_search_line(&text, 3, 0, 20, visible, sizeof(visible)) && !*visible);
    buf_free(&text);
    buf_free(&s->folder);
    buf_free(&s->query);
    buf_free(&s->view);
    s->report = NULL;
    s->initialized = false;
    for (unsigned i = 0; i < 2; i++)
        free(files[i].hits);
    phase = NORMAL;
    puts("OK six real folder-search two-pane layouts, all visible highlights/active hit, buttons and clipped "
         "UTF-8/tab content; no window or capture");
}

int main(void) {
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    current = a;
    Buffer b;
    char source[6000] = {0};
    for (int i = 0; i < 100; i++)
        strcat(source, "print(missing_name)\n");
    CHECK(buf_init(&b, source, "main.py"));
    CHECK(buf_init(&a->field, "/SDK été/", "field"));
    a->tabs[0] = &b;
    a->tab_count = 1;
    a->font_size = 20;
    a->char_w = 10;
    a->line_h = 30;
    a->tree_root_open = true;
    strcpy(a->project, "/project");
    strcpy(a->project_name, "Mon atelier");
    strcpy(a->python, "python3");
    strcpy(a->context_parent, a->project);
    strcpy(a->context_target, "/project/main.py");
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    a->analyses[0] = doc;
    doc->valid = true;
    doc->analyzed_version = b.version;
    doc->report.backend = LINT_BACKEND_PYLINT;
    doc->report.count = doc->report.total = 3;
    for (int i = 0; i < 3; i++) {
        Diagnostic *d = &doc->report.items[i];
        d->row = d->end_row = 2;
        d->column = 6;
        d->end_column = 18;
        d->severity = (LintSeverity)(2 - i);
        strcpy(d->code, i == 2 ? "E0602" : i == 1 ? "W0611" : "C0103");
        strcpy(d->message, "Message PyLint avec caractère accentué.");
    }
    doc->report.errors = doc->report.warnings = doc->report.infos = 1;
    lint_resolve(&doc->report, &b);
    for (int i = 0; i < 8; i++) {
        snprintf(a->tree[i].path, BUF_PATH, "/project/file_%d.py", i);
        snprintf(a->tree[i].name, sizeof(a->tree[i].name), "file_%d.py", i);
    }
    a->tree_count = 8;
    int frames = 0, panes = 0, sidebars = 0;
    const int widths[] = {960, 1100, 1440, 1920}, heights[] = {640, 900, 2160}, sides[] = {0, 200, 226, 420};
    for (int w = 0; w < 4; w++)
        for (int h = 0; h < 3; h++)
            for (int side = 0; side < 4; side++)
                for (int top = 0; top < 2; top++)
                    for (int split = 0; split < 2; split++) {
                        a->w = widths[w];
                        a->h = heights[h];
                        a->topbar = top != 0;
                        a->split = split != 0;
                        a->terminal = true;
                        a->lint_auto = true;
                        a->layout = layout_workspace(a->w, a->h, sides[side] > 0, a->topbar, true, false,
                                                     sides[side], 211);
                        a->ex = a->layout.sidebar_w;
                        a->code_y = a->layout.code_y;
                        a->code_bottom = a->layout.code_bottom;
                        a->code_h = a->layout.code_h;
                        a->terminal_y = (float)a->layout.console_y;
                        for (int pane = 0; pane < (split ? 2 : 1); pane++) {
                            a->active_pane = a->render_pane = pane;
                            a->pane_x = split && pane ? a->ex + (a->w - a->ex) / 2 + 3 : a->ex;
                            a->pane_right = split && !pane ? a->ex + (a->w - a->ex) / 2 - 3 : a->w;
                            a->code_x = a->pane_x + 76;
                            a->code_right = a->pane_right - 30;
                            a->ui.mouse = (Vector2){(float)a->code_x + 65, (float)a->code_y + 96};
                            reset(a, NORMAL);
                            diagnostic_gutter(a, &doc->report, 0, 5);
                            Hit *g = find(&a->ui, A_DIAGNOSTIC);
                            CHECK(g && g->arg == pane * LINT_MAX_ITEMS + 2);
                            segments = 0;
                            phase = UNDERLINE;
                            diagnostic_underlines(a, &doc->report, 0, 5);
                            CHECK(segments > 0 && last_wave == C_RED);
                            panes++;
                        }
                        a->active_pane = a->render_pane = 0;
                        if (a->ex > 0) {
                            reset(a, SIDEBAR);
                            opened_icon = -1;
                            sidebar(a);
                            CHECK(find(&a->ui, A_OPEN) && !find(&a->ui, A_NEW) && buttons == 3 &&
                                  opened_icon == I_FILE);
                            sidebars++;
                        }
                        reset(a, NORMAL);
                        a->console_mode = 1;
                        a->lint_scroll = 999;
                        console(a);
                        CHECK(find(&a->ui, A_DIAGNOSTIC));
                        CHECK(find(&a->ui, A_LINT_IGNORE) && a->lint_quick.buffer == &b &&
                              a->lint_quick.report == &doc->report &&
                              a->lint_quick.revision == doc->report.revision);
                        for (int qi = 0; qi < a->ui.hit_count; qi++)
                            if (a->ui.hits[qi].action == A_LINT_IGNORE) {
                                const Hit *q = &a->ui.hits[qi];
                                char code[6];
                                CHECK(lint_rule_decode(q->arg, code));
                                bool exact = false;
                                for (int di = 0; di < doc->report.count; di++)
                                    if (!strcmp(code, doc->report.items[di].code))
                                        exact = true;
                                CHECK(exact && q->rect.width >= 24 && q->rect.height >= 24 &&
                                      q->rect.x >= a->ex + 12 && q->rect.x + q->rect.width <= a->w - 16 &&
                                      q->rect.y >= a->terminal_y + 45 &&
                                      q->rect.y + q->rect.height <= a->layout.status_y - 8);
                                for (int ni = 0; ni < a->ui.hit_count; ni++)
                                    if (a->ui.hits[ni].action == A_DIAGNOSTIC) {
                                        Rectangle x = q->rect, y = a->ui.hits[ni].rect;
                                        CHECK(!(x.x < y.x + y.width && x.x + x.width > y.x &&
                                                x.y < y.y + y.height && x.y + x.height > y.y));
                                    }
                            }
                        for (int i = 0; i < a->ui.hit_count; i++)
                            if (a->ui.hits[i].action == A_DIAGNOSTIC)
                                CHECK(a->ui.hits[i].rect.y + a->ui.hits[i].rect.height <=
                                      a->layout.status_y - 8);
                        reset(a, STATUS);
                        status_bottom = 0;
                        status(a);
                        CHECK(status_bottom == a->h && !find(&a->ui, A_QUIT));
                        reset(a, TOPBAR);
                        topbar(a);
                        Hit *quit = find(&a->ui, A_QUIT), *effects = find(&a->ui, A_EFFECTS);
                        Hit *minimize = find(&a->ui, A_MINIMIZE), *collapse = find(&a->ui, A_TOPBAR);
                        CHECK(minimize && collapse && minimize != collapse && minimize->rect.x >= a->w - 44);
                        CHECK(minimize->rect.y == quit->rect.y &&
                              minimize->rect.x + minimize->rect.width <= a->w - 8);
                        for (int i = 0; i < a->ui.hit_count; i++)
                            if (a->ui.hits[i].action != A_MINIMIZE)
                                CHECK(a->ui.hits[i].rect.x + a->ui.hits[i].rect.width <= minimize->rect.x);
                        int minimize_count = 0;
                        for (int i = 0; i < a->ui.hit_count; i++)
                            minimize_count += a->ui.hits[i].action == A_MINIMIZE;
                        CHECK(minimize_count == 1);
                        CHECK(quit && effects && quit->rect.y == effects->rect.y);
                        CHECK(quit->rect.x > effects->rect.x + effects->rect.width &&
                              quit->rect.x + quit->rect.width <= a->w - 40);
                        for (int left = 0; left < a->ui.hit_count; left++) {
                            Rectangle r = a->ui.hits[left].rect;
                            CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= a->w &&
                                  r.y + r.height <= LAYOUT_TOPBAR_H);
                            for (int right = left + 1; right < a->ui.hit_count; right++) {
                                Rectangle t = a->ui.hits[right].rect;
                                CHECK(r.x + r.width <= t.x || t.x + t.width <= r.x || r.y + r.height <= t.y ||
                                      t.y + t.height <= r.y);
                            }
                        }
                        reset(a, CONTEXT);
                        menu_text_count = menu_icon_count = 0;
                        a->context_position = (Vector2){(float)a->w - 1, (float)a->h - 1};
                        context_menu(a);
                        CHECK(find(&a->ui, A_FS_NEW_FILE) && find(&a->ui, A_FS_NEW_FOLDER));
                        CHECK(menu_text_count == 5 && menu_icon_count == 5);
                        for (int mi = 1; mi < 5; mi++)
                            CHECK(menu_text_x[mi] == menu_text_x[0] && menu_icon_x[mi] == menu_icon_x[0]);
                        CHECK(find(&a->ui, A_FS_PASTE) && find(&a->ui, A_FS_DELETE));
                        reset(a, NORMAL);
                        settings(a);
                        CHECK(find(&a->ui, A_LINT_AUTO) && find(&a->ui, A_FONT_UP));
                        CHECK(scissors == 0);
                        frames++;
                    }
    int lint_rule_states = 0;
    const int lint_widths[] = {960, 1280, 1440, 1920}, lint_heights[] = {640, 720, 900, 1080};
    for (int screen = 0; screen < 4; screen++)
        for (int failed = 0; failed < 2; failed++) {
            a->w = lint_widths[screen];
            a->h = lint_heights[screen];
            a->modal = M_SETTINGS;
            reset(a, NORMAL);
            settings(a);
            const Hit *rule_button = find(&a->ui, A_LINT_RULES);
            CHECK(rule_button);
            for (int i = 0; i < a->ui.hit_count; i++)
                if (a->ui.hits[i].action == A_LINT_AUTO) {
                    Rectangle x = rule_button->rect, y = a->ui.hits[i].rect;
                    CHECK(!(x.x < y.x + y.width && x.x + x.width > y.x && x.y < y.y + y.height &&
                            x.y + x.height > y.y));
                }
            buf_free(&a->field);
            CHECK(buf_init(&a->field, "W0611,C0114", "règles"));
            snprintf(a->lint_rules_error, sizeof(a->lint_rules_error), "%s",
                     failed ? "Enregistrement impossible : accès refusé" : "");
            a->modal = M_LINT_RULES;
            reset(a, NORMAL);
            lint_rules_settings(a);
            CHECK(find(&a->ui, A_LINT_RULES_APPLY) && find(&a->ui, A_LINT_RULES_CLEAR) &&
                  find(&a->ui, A_MODAL_CLOSE));
            for (int i = 0; i < a->ui.hit_count; i++) {
                Rectangle r = a->ui.hits[i].rect;
                CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= a->w && r.y + r.height <= a->h);
            }
            CHECK(!scissors);
            lint_rule_states++;
        }
    printf("OK %d actual lint-rule dialog layouts: apply/clear/cancel, errors, bounds, nonoverlapping "
           "settings toggle/button; no window\n",
           lint_rule_states);
    int odoo_states = 0;
    for (int screen = 0; screen < 4; screen++)
        for (int enabled = 0; enabled < 2; enabled++) {
            a->w = lint_widths[screen];
            a->h = lint_heights[screen];
            a->odoo_draft_enabled = enabled != 0;
            snprintf(a->odoo_error, sizeof(a->odoo_error), "%s",
                     enabled ? "" : "Configuration invalide : plugin non disponible");
            a->modal = M_ODOO;
            reset(a, NORMAL);
            odoo_settings(a);
            CHECK(find(&a->ui, A_ODOO_TOGGLE) && find(&a->ui, A_ODOO_APPLY) && find(&a->ui, A_ODOO_CLEAR));
            for (int i = 0; i < a->ui.hit_count; i++) {
                Rectangle r = a->ui.hits[i].rect;
                CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= a->w && r.y + r.height <= a->h);
            }
            CHECK(!scissors);
            odoo_states++;
        }
    printf(
        "OK %d actual Odoo19 settings layouts: project toggle/ignored rules/apply/cancel/error, no window\n",
        odoo_states);
    int import_states = 0;
    const int import_widths[] = {960, 1280, 1440, 1920}, import_heights[] = {640, 720, 900, 1080};
    for (int screen = 0; screen < 4; screen++)
        for (int count = 0; count <= IMPORT_PATHS_MAX; count++)
            for (int failed = 0; failed < 2; failed++) {
                a->w = import_widths[screen];
                a->h = import_heights[screen];
                a->linter.imports.count = count;
                for (int i = 0; i < count; i++)
                    snprintf(a->linter.imports.paths[i], BUF_PATH, "/SDK été, ;'[]/framework-%d/python", i);
                strcpy(a->import_error, failed ? "Profil indisponible : accès refusé" : "");
                reset(a, NORMAL);
                a->modal = M_IMPORTS;
                imports_settings(a);
                CHECK((find(&a->ui, A_IMPORT_ADD) != NULL) == (count < IMPORT_PATHS_MAX));
                CHECK(find(&a->ui, A_SETTINGS) && find(&a->ui, A_MODAL_CLOSE));
                int removes = 0;
                for (int i = 0; i < a->ui.hit_count; i++) {
                    Hit *h = &a->ui.hits[i];
                    CHECK(h->rect.x >= 0 && h->rect.y >= 0 && h->rect.x + h->rect.width <= a->w &&
                          h->rect.y + h->rect.height <= a->h);
                    if (h->action == A_IMPORT_REMOVE) {
                        CHECK(h->arg == removes);
                        removes++;
                    }
                }
                CHECK(removes == count);
                reset(a, NORMAL);
                a->modal = M_SETTINGS;
                settings(a);
                CHECK(find(&a->ui, A_IMPORTS) && find(&a->ui, A_XML_PATHS));
                reset(a, NORMAL);
                a->xml_paths.count = count;
                a->xml_auto = failed == 0;
                for (int i = 0; i < count; i++)
                    snprintf(a->xml_paths.paths[i], BUF_PATH, "/sdk été/%d", i);
                strcpy(a->xml_paths_error, failed ? "Enregistrement impossible" : "");
                a->modal = M_XML_PATHS;
                xml_paths_settings(a);
                CHECK(find(&a->ui, A_XML_AUTO) && find(&a->ui, A_SETTINGS) && find(&a->ui, A_MODAL_CLOSE));
                CHECK((find(&a->ui, A_XML_PATH_ADD) != NULL) == (count < XML_PATHS_MAX));
                int xml_removes = 0, ups = 0, downs = 0;
                for (int i = 0; i < a->ui.hit_count; i++) {
                    Hit *h = &a->ui.hits[i];
                    CHECK(h->rect.x >= 0 && h->rect.y >= 0 && h->rect.x + h->rect.width <= a->w &&
                          h->rect.y + h->rect.height <= a->h);
                    if (h->action == A_XML_PATH_REMOVE)
                        xml_removes++;
                    if (h->action == A_XML_PATH_UP) {
                        CHECK(h->arg > 0 && h->arg < count);
                        ups++;
                    }
                    if (h->action == A_XML_PATH_DOWN) {
                        CHECK(h->arg >= 0 && h->arg + 1 < count);
                        downs++;
                    }
                }
                CHECK(xml_removes == count && ups == (count ? count - 1 : 0) && downs == ups);
                reset(a, NORMAL);
                a->modal = M_BROWSER;
                a->browser_mode = 4;
                browser(a);
                reset(a, NORMAL);
                a->modal = M_BROWSER;
                a->browser_mode = 3;
                a->browser_count = 1;
                strcpy(a->browser[0].name, "SDK été");
                a->browser[0].directory = true;
                browser(a);
                CHECK(find(&a->ui, A_BROWSER_SUBMIT) && find(&a->ui, A_BROWSER_UP) &&
                      find(&a->ui, A_BROWSER_ENTRY));
                CHECK(scissors == 0);
                import_states++;
            }
    printf("OK %d import-folder UI states: settings entry, 0..8 removable roots, errors, chooser, smallest "
           "960x640 window; actual view functions, no GUI\n",
           import_states);
    int snippet_states = 0, popup_states = 0;
    char error[512];
    CHECK(buf_init(&a->snippet_trigger, "xml_form", "trigger"));
    CHECK(buf_init(&a->snippet_title, "Vue Odoo · été", "title"));
    char body[8192] = "<form>\n";
    for (int i = 0; i < 100; i++)
        strcat(body, "\t<field name=\"été\"/>\n");
    strcat(body, "</form>");
    CHECK(buf_init(&a->snippet_body, body, "code"));
    buf_select_all(&a->snippet_body);
    for (int screen = 0; screen < 4; screen++) {
        a->w = import_widths[screen];
        a->h = import_heights[screen];
        a->layout = layout_workspace(a->w, a->h, true, true, true, false, 226, 211);
        for (int count = 0; count <= SNIPPETS_MAX; count++) {
            snippets_free(&a->snippets);
            for (int i = 0; i < count; i++) {
                char name[64];
                snprintf(name, sizeof(name), "xml_macro_%02d", i);
                CHECK(snippets_put(&a->snippets, -1, name, "Macro globale été", "<x/>\n$0", error,
                                   sizeof(error)));
            }
            for (int failed = 0; failed < 2; failed++) {
                strcpy(a->snippet_error, failed ? "Écriture impossible : accès refusé" : "");
                a->snippet_scroll = 999;
                a->snippet_index = count ? count - 1 : -1;
                reset(a, NORMAL);
                a->modal = M_SNIPPETS;
                snippets_view(a);
                CHECK(find(&a->ui, A_SETTINGS) && find(&a->ui, A_MODAL_CLOSE));
                CHECK((find(&a->ui, A_SNIPPET_NEW) != NULL) == (count < SNIPPETS_MAX));
                int deletes = 0;
                for (int i = 0; i < a->ui.hit_count; i++) {
                    Hit *h = &a->ui.hits[i];
                    CHECK(h->rect.x >= 0 && h->rect.y >= 0 && h->rect.x + h->rect.width <= a->w &&
                          h->rect.y + h->rect.height <= a->h);
                    if (h->action == A_SNIPPET_DELETE)
                        deletes++;
                }
                CHECK(deletes == (count < 8 ? count : 8));
                for (int modal = 0; modal < 3; modal++) {
                    reset(a, NORMAL);
                    a->modal = modal == 0 ? M_SNIPPET_EDIT : modal == 1 ? M_SNIPPET_DIRTY : M_SNIPPET_DELETE;
                    if (!modal) {
                        a->snippet_focus = 2;
                        a->snippet_body.scroll_y = 99999;
                        snippet_editor(a);
                        CHECK(find(&a->ui, A_SNIPPET_FOCUS) && find(&a->ui, A_SNIPPET_SAVE));
                    } else {
                        snippet_confirm(a);
                        CHECK(find(&a->ui, modal == 1 ? A_SNIPPET_CONTINUE : A_SNIPPET_CONFIRM_DELETE));
                    }
                    for (int i = 0; i < a->ui.hit_count; i++) {
                        Rectangle r = a->ui.hits[i].rect;
                        CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= a->w && r.y + r.height <= a->h);
                    }
                    CHECK(!scissors);
                    snippet_states++;
                }
            }
        }
        for (int count = 0; count <= COMPLETION_MAX; count++)
            for (int bottom = 0; bottom < 2; bottom++) {
                reset(a, NORMAL);
                a->modal = M_NONE;
                a->code_x = bottom ? a->w - 20 : 250;
                a->code_y = bottom ? a->layout.status_y - 28 : 120;
                b.cursor = 0;
                a->completion_count = count;
                a->completion_index = count ? count - 1 : 0;
                for (int i = 0; i < count; i++) {
                    snprintf(a->completions[i].name, sizeof(a->completions[i].name), "xml_suggestion_%02d",
                             i);
                    strcpy(a->completions[i].detail, "Macro globale · bibliothèque commune");
                    a->completions[i].kind = "SDPC"[i % 4];
                }
                strcpy(a->completion_status, bottom ? "Index Odoo partiel : limite atteinte" : "");
                completions(a);
                Rectangle r = a->completion_rect;
                CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= a->w && r.y + r.height <= a->h);
                int proposals = 0;
                for (int i = 0; i < a->ui.hit_count; i++)
                    if (a->ui.hits[i].action == A_COMPLETION) {
                        CHECK(a->ui.hits[i].arg >= 0 && a->ui.hits[i].arg < count);
                        proposals++;
                    }
                CHECK(proposals == (count < 7 ? count : 7));
                popup_states++;
            }
    }
    printf("OK %d snippet dialog states (0..64 entries, errors, editor/dirty/delete) and %d completion "
           "popups (0..32 items, loading, partial, window edges), actual views; no GUI\n",
           snippet_states, popup_states);
    snippets_free(&a->snippets);
    buf_free(&a->snippet_trigger);
    buf_free(&a->snippet_title);
    buf_free(&a->snippet_body);
    /* Actual XML picker at every display size: loading/error/empty/filter/end of 4096 IDs. */
    a->xml.refs.items = calloc(XML_REF_IDS_MAX, sizeof(XmlIdentifier));
    CHECK(a->xml.refs.items);
    int xml_states = 0;
    const int xml_counts[] = {0, 1, 8, 100, XML_REF_IDS_MAX};
    for (int screen = 0; screen < 4; screen++) {
        a->w = import_widths[screen];
        a->h = import_heights[screen];
        for (int c = 0; c < 5; c++)
            for (int mode = 0; mode < 4; mode++)
                for (int edge = 0; edge < 3; edge++) {
                    int count = xml_counts[c];
                    a->xml.refs.count = count;
                    a->xml.refs.raw = mode < 2 ? NULL : (char *)"loaded";
                    strcpy(a->xml.error, mode == 1 ? "Fichier XML introuvable" : "");
                    a->xml.count = mode == 2 ? count : 0;
                    a->xml.index = edge == 0      ? 0
                                   : edge == 1    ? a->xml.count / 2
                                   : a->xml.count ? a->xml.count - 1
                                                  : 0;
                    for (int i = 0; i < count; i++) {
                        snprintf(a->xml.refs.items[i].id, sizeof(a->xml.refs.items[i].id), "view_%d_été", i);
                        strcpy(a->xml.refs.items[i].tag, "record");
                        strcpy(a->xml.refs.items[i].model, "ir.ui.view");
                        a->xml.refs.items[i].line = i + 1;
                        a->xml.filtered[i] = i;
                    }
                    reset(a, NORMAL);
                    a->modal = M_XML_IDS;
                    xml_dialog(a);
                    CHECK(find(&a->ui, A_MODAL_CLOSE));
                    CHECK((find(&a->ui, A_XML_PICK) != NULL) == (a->xml.count > 0));
                    for (int i = 0; i < a->ui.hit_count; i++) {
                        Hit *hit = &a->ui.hits[i];
                        if (hit->action == A_XML_PICK)
                            CHECK(hit->rect.y >= 0 && hit->rect.y + hit->rect.height <= a->h - 12 &&
                                  hit->arg >= 0 && hit->arg < a->xml.count);
                    }
                    CHECK(!scissors);
                    xml_states++;
                }
    }
    free(a->xml.refs.items);
    memset(&a->xml, 0, sizeof(a->xml));
    printf("OK %d actual XML picker states (loading/error/empty/filter/4096 IDs), menu fixed icon/text "
           "columns; no GUI\n",
           xml_states);
    /* Real problem-list renderer: syntax/internal reports are never hideable. */
    for (int backend = LINT_BACKEND_SYNTAX; backend <= LINT_BACKEND_ERROR; backend++) {
        reset(a, NORMAL);
        doc->report.backend = (LintBackend)backend;
        doc->report.count = 1;
        strcpy(doc->report.items[0].code, "E0001");
        strcpy(doc->report.items[0].symbol, "syntax-error");
        problem_rows(a, &doc->report, a->terminal_y, (float)a->layout.status_y);
        CHECK(!find(&a->ui, A_LINT_IGNORE));
    }
    doc->report.backend = LINT_BACKEND_PYLINT;
    /* Missing backend and stale results are explicit, not green all-clear. */
    reset(a, NORMAL);
    doc->report.count = doc->report.total = 0;
    doc->report.backend = LINT_BACKEND_SYNTAX;
    strcpy(doc->report.status, "PyLint non installé");
    problem_rows(a, &doc->report, a->terminal_y, (float)a->layout.status_y);
    b.version++;
    CHECK(app_diagnostics(a) == NULL);
    /* Actual native SSH views: masked UTF-8, all profiles/connection states. */
    for (int i = 0; i < 5; i++)
        CHECK(buf_init(&a->ssh_fields[i], i == 0 ? "odoo-server" : "", "ssh field"));
    CHECK(buf_init(&a->ssh_auth, "secret-é-jamais-dessiner", "ssh auth"));
    forbidden_secret = "secret-é-jamais-dessiner";
    for (int size = 0; size < 3; size++) {
        a->w = size == 0 ? 960 : size == 1 ? 1280 : 1920;
        a->h = size == 0 ? 640 : size == 1 ? 720 : 1080;
        for (int state = SSH_IDLE; state <= SSH_ERROR; state++) {
            reset(a, NORMAL);
            a->ssh.state = (SshState)state;
            a->modal = M_SSH;
            app_ssh_draw(a);
            CHECK(find(&a->ui, A_MODAL_CLOSE));
            CHECK(a->ssh.state == SSH_DISCONNECTING || find(&a->ui, A_SSH_CONNECT) ||
                  find(&a->ui, A_SSH_CANCEL) || find(&a->ui, A_SSH_DISCONNECT));
        }
        a->ssh_profiles.count = SSH_PROFILES_MAX;
        for (int i = 0; i < SSH_PROFILES_MAX; i++) {
            ssh_profile_init(&a->ssh_profiles.items[i]);
            strcpy(a->ssh_profiles.items[i].host, "odoo-alias");
        }
        reset(a, NORMAL);
        a->modal = M_SSH_PROFILES;
        app_ssh_draw(a);
        CHECK(find(&a->ui, A_SSH_PROFILE_PICK));
        CHECK(find(&a->ui, A_SSH_PROFILE_REMOVE));
        for (int confirm = 0; confirm < 2; confirm++) {
            reset(a, NORMAL);
            a->modal = M_SSH_AUTH;
            a->ssh.auth_confirm = confirm != 0;
            strcpy(a->ssh.prompt,
                   confirm ? "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" : "Password for odoo:");
            app_ssh_draw(a);
            CHECK(find(&a->ui, confirm ? A_SSH_AUTH_YES : A_SSH_AUTH_SEND));
        }
    }
    forbidden_secret = NULL;
    folder_search_views(a);
    buf_free(&a->ssh_auth);
    for (int i = 0; i < 5; i++)
        buf_free(&a->ssh_fields[i]);
    free(doc);
    buf_free(&b);
    buf_free(&a->field);
    free(a);
    printf("OK %d assertions: %d UI states, %d diagnostic panes, %d Open-file explorer headers, clickable "
           "Problems, error-priority waves, Quit/footer, context creation and settings; no GUI/preview\n",
           checks, frames, panes, sidebars);
    return 0;
}
