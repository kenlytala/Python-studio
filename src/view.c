#include "app_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOX(x, y, w, h, t) ui_box((float)(x), (float)(y), (float)(w), (float)(h), t, 0, 255)
#define RECT(x, y, w, h) ((Rectangle){(float)(x), (float)(y), (float)(w), (float)(h)})
static const Tone syntax_tone[] = {C_TEXT,       C_KEYWORD, C_STRING,   C_NUMBER,  C_COMMENT,
                                   C_DEFINITION, C_BUILTIN, C_OPERATOR, C_CONSTANT};
static void text(App *a, const char *s, float x, float y, float size, Tone t) {
    ui_text(&a->ui, s, x, y, size, t, false);
}
static void fit(App *a, const char *s, float x, float y, float width, float size, Tone tone) {
    ui_text_fit(&a->ui, s, x, y, width, size, tone, false);
}
static void mono(App *a, const char *s, float x, float y, float size, Tone t) {
    ui_text(&a->ui, s, x, y, size, t, true);
}
static void label(App *a, const char *s, float x, float y, float size, Tone t) {
    ui_label(&a->ui, s, x, y, size, t);
}
static void icon(App *a, Icon i, float x, float y, float size, Tone t) {
    ui_icon(&a->ui, i, x, y, size, t, 255);
}
static void button(App *a, Rectangle r, Action id, int arg, const char *s, int i, int style, bool active,
                   const char *tip) {
    ui_button(&a->ui, r, id, arg, s, i, style, active, tip);
}
static void hit(App *a, Rectangle r, Action id, int arg, const char *tip) {
    ui_hit(&a->ui, r, id, arg, tip);
}
static bool python(Buffer *b) {
    return IsFileExtension(b->name, ".py") || IsFileExtension(b->name, ".pyw");
}

void app_field_draw(App *a, Buffer *b, Rectangle r, bool focused, const char *placeholder, Action id,
                    bool subtle) {
    if (!subtle) {
        ui_box(r.x, r.y, r.width, r.height, C_BG, 5, 255);
        ui_outline(r.x, r.y, r.width, r.height, focused ? C_ACCENT : C_BORDER, 5, focused ? 100 : 255);
    }
    size_t start = 0;
    float available = r.width - 28;
    size_t col = buf_column(b, b->cursor),
           visible = (size_t)fmaxf(1, available / ui_width(&a->ui, "M", 12, true));
    if (col > visible)
        start = buf_position(b, 0, col - visible);
    BeginScissorMode((int)(r.x + 8), (int)(r.y + 2), (int)(r.width - 16), (int)(r.height - 4));
    size_t left, right;
    if (focused && buf_selection(b, &left, &right)) {
        float x =
            r.x + 11 +
            (float)((long)buf_column(b, left) - (long)buf_column(b, start)) * ui_width(&a->ui, "M", 12, true);
        float width = (float)(buf_column(b, right) - buf_column(b, left)) * ui_width(&a->ui, "M", 12, true);
        ui_box(x, r.y + 5, width, r.height - 10, C_BLUE, 2, 45);
    }
    mono(a, b->len ? b->text + start : placeholder, r.x + 11, r.y + (r.height - ui_text_size(12)) / 2 - 1, 12,
         b->len ? C_TEXT : C_DIM);
    if (focused && ((int)(GetTime() * 2) % 2 == 0)) {
        float x = r.x + 11 + (float)(col - buf_column(b, start)) * ui_width(&a->ui, "M", 12, true);
        BOX(x, r.y + (r.height - 20) / 2, 1, 20, C_ACCENT);
    }
    EndScissorMode();
    if (id != A_NONE)
        hit(a, r, id, 0, NULL);
}
static void topbar(App *a) {
    BOX(0, 0, a->w, 69, C_PANEL);
    ui_line(0, 68, (float)a->w, 68, C_BORDER, 1, 255);
    if (a->effects)
        DrawCircleGradient(39, 32, 39, ui_alpha(C_ACCENT, 12), ui_alpha(C_ACCENT, 0));
    icon(a, I_PRISM, 24, 23, 27, C_ACCENT);
    text(a, "Python Studio", 64, 21, 22, C_WHITE);
    BOX(228, 25, 1, 20, C_BORDER);
    label(a, "ÉDITION C", 248, 23, 8, C_MUTED);
    label(a, "C11 / RAYLIB", 248, 39, 7, C_ACCENT);
    float cx = fmaxf(328, (float)a->w / 2 - 166), cw = fminf(340, (float)a->w - cx - 388);
    if (cw > 100) {
        ui_box(cx, 19, cw, 32, C_BG, 6, 255);
        ui_outline(cx, 19, cw, 32, C_BORDER, 6, 255);
        icon(a, I_SEARCH, cx + 12, 27, 15, C_DIM);
        fit(a, cw > 260 ? "Rechercher une commande…" : "Commandes…", cx + 36, 27, cw - 111, 11, C_MUTED);
        ui_box(cx + cw - 63, 23, 52, 24, C_RAISED, 3, 255);
        text(a, "Ctrl K", cx + cw - 57, 26, 9, C_MUTED);
        hit(a, RECT(cx, 19, cw, 32), A_COMMANDS, 0, "Palette de commandes · Ctrl+K");
    }
    button(a, RECT(a->w - 370, 20, 28, 31), A_TOPBAR, 0, NULL, I_UP, 0, false,
           "Replier le menu du haut · F8");
    button(a, RECT(a->w - 332, 20, 32, 31), A_SPLIT, 0, NULL, I_SPLIT, 0, a->split,
           "Deux fichiers côte à côte · F7 / Ctrl+\\");
    button(a, RECT(a->w - 252, 20, 93, 31), A_EFFECTS, 0, "Effets", I_SPARK, 0, a->effects,
           "Effets visuels · F6");
    if (a->effects)
        DrawCircle(a->w - 168, 35, 2, ui_alpha(C_ACCENT, 130 + (int)(sinf((float)GetTime() * 2) * 50)));
    // Groupe fenêtre à l'extrême droite : Plein écran → Réduire → Quitter (icônes seules, sans texte)
    button(a, RECT(a->w - 114, 20, 32, 31), A_FULLSCREEN, 0, NULL, I_EXPAND, 0, IsWindowFullscreen(),
           "Plein écran · F11");
    button(a, RECT(a->w - 76, 20, 32, 31), A_MINIMIZE, 0, NULL, I_MINUS, 1, false,
           "Réduire l’éditeur sans quitter");
    button(a, RECT(a->w - 38, 20, 32, 31), A_QUIT, 0, NULL, I_CLOSE, 3, false,
           "Quitter l’éditeur · Alt+F4 (propose d’enregistrer les modifications)");
}
static bool folder_expanded(App *a, const char *path) {
    for (int i = 0; i < a->expanded_count; i++)
        if (!strcmp(a->expanded[i], path))
            return true;
    return false;
}
static void sidebar(App *a) {
    int x = 0, w = a->ex, top = a->layout.topbar_h, bottom_y = a->layout.status_y;
    Buffer *b = app_buffer(a);
    BOX(x, top, w, bottom_y - top, C_PANEL);
    ui_line(x + w - 1, top, x + w - 1, (float)bottom_y, C_BORDER, 1, 255);
    text(a, "Dossiers", x + 18, top + 23, 14, C_TEXT);
    button(a, RECT(w - 96, top + 17, 24, 26), A_OPEN, 0, NULL, I_FILE, 0, false,
           "Ouvrir un fichier · Ctrl+O");
    button(a, RECT(w - 66, top + 17, 24, 26), A_FOLDER, 0, NULL, I_FOLDER_OPEN, 0, false,
           "Ouvrir un dossier · Ctrl+Maj+O");
    button(a, RECT(w - 36, top + 17, 24, 26), A_SIDEBAR, 0, NULL, I_LEFT, 0, false,
           "Replier l'explorateur · Ctrl+B");
    icon(a, a->tree_root_open ? I_DOWN : I_RIGHT, x + 16, top + 68, 14, C_MUTED);
    icon(a, I_FOLDER, x + 37, top + 67, 17, C_ACCENT);
    BeginScissorMode(x + 62, top + 63, w - 72, 25);
    fit(a, a->project_name, x + 64, top + 66, w - 77, 12, C_WHITE);
    EndScissorMode();
    hit(a, RECT(x + 10, top + 59, w - 20, 33), A_TREE_ROOT, 0, NULL);
    TreeLayout tree = layout_tree(&a->layout);
    BeginScissorMode(x + 1, tree.top, w - 2, tree.bottom - tree.top);
    if (a->tree_root_open) {
        for (int j = 0; j < tree.rows && a->tree_scroll + j < a->tree_count; j++) {
            int i = a->tree_scroll + j;
            Entry *e = &a->tree[i];
            float y = tree.first_y + (float)j * LAYOUT_TREE_ROW_H, indent = (float)e->depth * 13;
            bool selected = !strcmp(b->path, e->path),
                 hover = ui_inside(RECT(x + 10, y, w - 20, 31), a->ui.mouse);
            bool context_selected = a->modal == M_CONTEXT && !strcmp(a->context_target, e->path);
            if (selected || hover || context_selected)
                ui_box(x + 11, y, w - 23, 30, selected ? C_ACCENT : C_RAISED, 5, selected ? 12 : 255);
            if (selected)
                ui_box(x + 12, y + 9, 2, 12, C_ACCENT, 1, 255);
            if (e->directory)
                icon(a, folder_expanded(a, e->path) ? I_DOWN : I_RIGHT, x + 21 + indent, y + 9, 12, C_MUTED);
            icon(a,
                 e->directory                      ? I_FOLDER
                 : IsFileExtension(e->name, ".py") ? I_PYTHON
                                                   : I_FILE,
                 x + 39 + indent, y + 7, 17, C_LAVENDER);
            fit(a, e->name, x + 65 + indent, y + 7, w - 85 - indent, 12, selected ? C_ACCENT : C_MUTED);
            if (selected && buf_dirty(b))
                DrawCircle(x + w - 24, (int)(y + 16), 2.5f, ui_colors[C_ACCENT]);
            hit(a, RECT(x + 12, y, w - 24, LAYOUT_TREE_ITEM_H), A_TREE, i, NULL);
        }
    }
    EndScissorMode();
}
static void findbar(App *a) {
    int y = a->layout.find_y;
    BOX(a->ex, y, a->w - a->ex, 43, C_PANEL);
    ui_line((float)a->ex, y + 42, (float)a->w, y + 42, C_BORDER, 1, 255);
    char count[64];
    snprintf(count, sizeof(count), "%d / %d", a->match_count ? a->match_index + 1 : 0, a->match_count);
    float count_w = ui_width(&a->ui, count, 12, false);
    float w = fminf(380, (float)(a->w - a->ex) - count_w - 192);
    app_field_draw(a, &a->search, RECT(a->ex + 20, y + 7, w, 29), a->focus == 1,
                   "Rechercher dans le fichier…", A_FIELD_FIND, false);
    float count_x = a->ex + w + 36;
    text(a, count, count_x, y + 13, 12, C_MUTED);
    button(a, RECT(count_x + count_w + 12, y + 8, 25, 26), A_FIND_PREV, 0, NULL, I_UP, 0, false,
           "Précédent · Maj+Entrée");
    button(a, RECT(count_x + count_w + 43, y + 8, 25, 26), A_FIND_NEXT, 0, NULL, I_DOWN, 0, false,
           "Suivant · Entrée");
    button(a, RECT(a->w - 44, y + 8, 26, 26), A_FIND_CLOSE, 0, NULL, I_CLOSE, 0, false, "Fermer · Échap");
}
static void tabs(App *a) {
    int x = a->ex, y = a->layout.tabs_y, cy = a->layout.crumb_y;
    BOX(x, y, a->w - x, 47, C_PANEL);
    ui_line((float)x, y + 46, (float)a->w, y + 46, C_BORDER, 1, 255);
    TabBarLayout bar = layout_tabbar(&a->layout, a->tab_count, a->active);
    float tx = (float)x;
    for (int i = bar.first; i < bar.first + bar.count; i++) {
        Buffer *b = a->tabs[i];
        bool active = i == a->active;
        if (a->split && i == a->panes[1 - a->active_pane].tab && !active)
            BOX(tx + 14, y + 43, 145, 2, C_BLUE);
        if (active) {
            BOX(tx, y, LAYOUT_TAB_W, LAYOUT_TABS_H, C_RAIL);
            BOX(tx + 14, y, 145, 2, C_ACCENT);
        }
        icon(a, python(b) ? I_PYTHON : I_FILE, tx + 18, y + 15, 17, C_LAVENDER);
        BeginScissorMode((int)(tx + 43), y + 11, 98, 27);
        fit(a, b->name, tx + 44, y + 15, 92, 12, active ? C_WHITE : C_MUTED);
        EndScissorMode();
        hit(a, RECT(tx, y + 3, 146, 41), A_TAB, i, NULL);
        if (buf_dirty(b)) {
            DrawCircle((int)(tx + 151), y + 23, 3, ui_colors[C_ACCENT]);
            hit(a, RECT(tx + 140, y + 9, 24, 28), A_TAB_CLOSE, i, "Fermer l'onglet · Ctrl+W");
        } else
            button(a, RECT(tx + 140, y + 10, 24, 27), A_TAB_CLOSE, i, NULL, I_CLOSE, 0, false,
                   "Fermer l'onglet · Ctrl+W");
        ui_line(tx + LAYOUT_TAB_W, y + 11, tx + LAYOUT_TAB_W, y + 37, C_BORDER, 1, 150);
        tx += LAYOUT_TAB_W;
    }
    button(a, RECT(bar.new_x, y + 10, LAYOUT_TAB_ADD_W, 27), A_NEW, 0, NULL, I_PLUS, 0, false,
           "Nouveau fichier · Ctrl+N");
    button(a, RECT(bar.run_x, y + 7, LAYOUT_TAB_RUN_W, 33), a->runner.running ? A_STOP : A_RUN, 0,
           a->runner.running ? "Arrêter" : "Exécuter", a->runner.running ? I_STOP : I_PLAY,
           a->runner.running ? 3 : 2, false,
           a->runner.running ? "Arrêter · Maj+F5" : "Exécuter le fichier Python actif · F5");
    button(a, RECT(bar.more_x, y + 10, LAYOUT_TAB_MORE_W, 27), A_COMMANDS, 0, NULL, I_MORE, 0, false,
           "Toutes les commandes");
    BOX(x, cy, a->w - x, 35, C_RAIL);
    icon(a, I_FOLDER, x + 22, cy + 10, 15, C_DIM);
    float project_w = fminf(160, ui_width(&a->ui, a->project_name, 10, false));
    fit(a, a->project_name, x + 43, cy + 9, project_w, 10, C_MUTED);
    float nx = x + 53 + project_w;
    icon(a, I_RIGHT, nx, cy + 11, 12, C_DIM);
    icon(a, python(app_buffer(a)) ? I_PYTHON : I_FILE, nx + 24, cy + 9, 15, C_MUTED);
    float right = a->w - (a->w - x > 750 ? 122 : 18);
    fit(a, app_buffer(a)->name, nx + 47, cy + 9, right - nx - 47, 10, C_TEXT);
    if (a->w - x > 750)
        label(a, "PYTHON 3", (float)a->w - 111, cy + 10, 8, C_DIM);
    if (a->find_open)
        findbar(a);
}
static void pane_header(App *a, int pane) {
    float x = (float)a->pane_x, w = (float)(a->pane_right - a->pane_x);
    int cy = a->layout.crumb_y;
    bool active = pane == a->active_pane;
    BOX(x, cy, w, 35, C_PANEL);
    ui_box(x + 1, cy, w - 2, 2, active ? C_ACCENT : C_BORDER, 0, active ? 160 : 255);
    icon(a, python(app_buffer(a)) ? I_PYTHON : I_FILE, x + 15, cy + 10, 14, active ? C_ACCENT : C_MUTED);
    BeginScissorMode((int)x + 36, cy + 3, (int)w - 105, 28);
    fit(a, app_buffer(a)->name, x + 38, cy + 9, w - 123, 11, active ? C_WHITE : C_MUTED);
    EndScissorMode();
    label(a, pane ? "DROITE" : "GAUCHE", x + w - 78, cy + 10, 7, active ? C_ACCENT : C_DIM);
    hit(a, RECT(x, cy + 1, w, 34), A_PANE_FOCUS, pane, "Activer ce volet · Ctrl+1 / Ctrl+2");
}
static void range_draw(App *a, size_t start, size_t end, Tone t, int alpha) {
    Buffer *b = app_buffer(a);
    size_t first = buf_row(b, start), last = buf_row(b, end);
    int top = (int)(b->scroll_y / a->line_h) - 1;
    if (top < 0)
        top = 0;
    if (first < (size_t)top)
        first = (size_t)top;
    size_t bottom = (size_t)((b->scroll_y + (float)a->code_h) / a->line_h) + 2;
    if (last > bottom)
        last = bottom;
    for (size_t row = first; row <= last && row < b->line_count; row++) {
        size_t left = row == buf_row(b, start) ? buf_column(b, start) : 0;
        size_t right = row == buf_row(b, end) ? buf_column(b, end) : buf_column(b, buf_line_end(b, row)) + 1;
        float x = (float)a->code_x + (float)left * a->char_w - b->scroll_x;
        float xend =
            fminf((float)a->code_right, x + fmaxf(a->char_w * 0.5f, (float)(right - left) * a->char_w));
        x = fmaxf((float)a->code_x - 1, x);
        float y = (float)a->code_y + 9 + (float)row * a->line_h - b->scroll_y;
        ui_box(x, y, xend - x, a->line_h - 1, t, 2, alpha);
    }
}
static void code_line(App *a, size_t row, float y) {
    Buffer *b = app_buffer(a);
    size_t p = b->lines[row], end = buf_line_end(b, row);
    int col = 0;
    char run[2048];
    size_t length = 0;
    int runcol = 0;
    unsigned kind = SYN_TEXT;
    float mincol = b->scroll_x / a->char_w - 1,
          maxcol = (float)(a->code_right - a->code_x) / a->char_w + mincol + 3;
    size_t indent = p;
    while (indent < end && (b->text[indent] == ' ' || b->text[indent] == '\t'))
        indent++;
    int depth = (int)buf_column(b, indent);
    for (int level = 4; level <= depth && level < 500; level += 4) {
        float x = (float)a->code_x + (float)(level - 4) * a->char_w - b->scroll_x;
        ui_line(x, y - 3, x, y + a->line_h - 3, C_BORDER, 1, 120);
    }
    while (p < end && (float)col < maxcol) {
        size_t next = utf8_next(b->text, b->len, p), n = next - p;
        unsigned k = a->colors && p < a->colors_capacity ? a->colors[p] : SYN_TEXT;
        if (k > 8)
            k = 0;
        int width = b->text[p] == '\t' ? 4 - col % 4 : 1;
        if ((float)(col + width) >= mincol) {
            if (length && (k != kind || length + n + 5 >= sizeof(run))) {
                run[length] = 0;
                mono(a, run, (float)a->code_x + (float)runcol * a->char_w - b->scroll_x, y,
                     (float)a->font_size, syntax_tone[kind]);
                length = 0;
            }
            if (!length) {
                runcol = col;
                kind = k;
            }
            if (b->text[p] == '\t') {
                for (int i = 0; i < width; i++)
                    run[length++] = ' ';
            } else {
                memcpy(run + length, b->text + p, n);
                length += n;
            }
        }
        col += width;
        p = next;
    }
    if (length) {
        run[length] = 0;
        mono(a, run, (float)a->code_x + (float)runcol * a->char_w - b->scroll_x, y, (float)a->font_size,
             syntax_tone[kind]);
    }
}
static void minimap(App *a) {
    Buffer *b = app_buffer(a);
    float x = (float)a->pane_right - 91, y = (float)a->code_y + 18, w = 71;
    label(a, "APERÇU", x + 3, y, 7, C_DIM);
    y += 26;
    float step = fminf(3.6f, fmaxf(0.5f, (float)(a->code_h - 62) / (float)b->line_count));
    for (size_t row = 0; row < b->line_count && row < 1000; row++) {
        size_t end = buf_line_end(b, row);
        float col = 0;
        for (size_t p = b->lines[row]; p < end && col < 90;) {
            size_t next = utf8_next(b->text, b->len, p);
            unsigned k = a->colors && p < a->colors_capacity ? a->colors[p] : 0;
            if (k > 8)
                k = 0;
            if (b->text[p] != ' ' && b->text[p] != '\t')
                ui_box(x + col * 0.77f, y + (float)row * step, 0.9f, 1.1f, syntax_tone[k], 0, 80);
            col += (float)(b->text[p] == '\t' ? 4 - (int)col % 4 : 1);
            p = next;
        }
    }
    float top = b->scroll_y / a->line_h,
          h = fminf((float)a->code_h - 55, fmaxf(20, (float)a->code_h / a->line_h * step));
    ui_box(x - 4, y + top * step - 4, w + 9, h, C_ACCENT, 3, 6);
    ui_outline(x - 4, y + top * step - 4, w + 9, h, C_ACCENT, 3, 22);
    hit(a, RECT(x - 6, y - 5, w + 15, a->code_h - 50), A_MINIMAP, 0, "Naviguer dans le fichier");
}
static Tone diagnostic_tone(const Diagnostic *d) {
    return d->severity == LINT_ERROR ? C_RED : d->severity == LINT_WARNING ? C_NUMBER : C_BLUE;
}
static void diagnostic_gutter(App *a, LintReport *report, int first, int last) {
    if (!report)
        return;
    for (int row = first; row < last; row++) {
        int best = -1;
        for (int i = 0; i < report->count; i++)
            if (report->items[i].row == (size_t)row &&
                (best < 0 || report->items[i].severity < report->items[best].severity))
                best = i;
        if (best < 0)
            continue;
        Diagnostic *d = &report->items[best];
        float y = (float)a->code_y + 13 + (float)row * a->line_h - app_buffer(a)->scroll_y;
        if (y < a->code_y || y + 17 > a->code_bottom)
            continue;
        DrawCircle(a->pane_x + 57, (int)y + 9, 3.2f, ui_colors[diagnostic_tone(d)]);
        hit(a, RECT(a->pane_x + 51, y, 12, 20), A_DIAGNOSTIC, a->render_pane * LINT_MAX_ITEMS + best,
            d->message);
    }
}
static void diagnostic_underlines(App *a, LintReport *report, int first, int last) {
    if (!report)
        return;
    Buffer *b = app_buffer(a);
    for (int severity = LINT_INFO; severity >= LINT_ERROR; severity--) {
        int segments = 0;
        for (int i = 0; i < report->count; i++) {
            Diagnostic *d = &report->items[i];
            if (d->severity != (LintSeverity)severity)
                continue;
            if (d->row >= (size_t)last || d->end_row < (size_t)first)
                continue;
            size_t top = d->row > (size_t)first ? d->row : (size_t)first;
            size_t end = d->end_row < (size_t)(last - 1) ? d->end_row : (size_t)(last - 1);
            for (size_t row = top; row <= end; row++) {
                size_t left = row == d->row ? d->visual_column : 0;
                size_t right = row == d->end_row ? d->end_visual_column : buf_column(b, buf_line_end(b, row));
                if (right <= left)
                    right = left + 1;
                float x = (float)a->code_x + (float)left * a->char_w - b->scroll_x;
                float to = (float)a->code_x + (float)right * a->char_w - b->scroll_x;
                x = fmaxf((float)a->code_x, x);
                to = fminf((float)a->code_right - 2, to);
                float y =
                    (float)a->code_y + 13 + (float)row * a->line_h - b->scroll_y + (float)a->font_size + 2;
                if (to <= x || y < a->code_y || y + 3 > a->code_bottom)
                    continue;
                for (float xx = x; xx < to && segments < 4096; xx += 6) {
                    float mid = fminf(to, xx + 3), finish = fminf(to, xx + 6);
                    ui_line(xx, y, mid, y + 2, diagnostic_tone(d), 1.2f, 235);
                    ui_line(mid, y + 2, finish, y, diagnostic_tone(d), 1.2f, 235);
                    segments++;
                }
                if (ui_inside(RECT(x, y - 2, to - x, 7), a->ui.mouse))
                    hit(a, RECT(x, y - 2, to - x, 7), A_DIAGNOSTIC, a->render_pane * LINT_MAX_ITEMS + i,
                        d->message);
            }
        }
    }
}
static void diagnostic_summary(App *a, LintReport *report, char *out, size_t cap) {
    if (!python(app_buffer(a)))
        snprintf(out, cap, "Diagnostics réservés aux fichiers Python.");
    else if (!report) {
        LintDocument *doc = a->analyses[a->active];
        snprintf(out, cap, "%s",
                 a->linter.process.running && a->linter.owner == doc ? "Analyse Python en cours…"
                 : a->lint_auto || (a->lint_force && a->lint_force == doc)
                     ? "Analyse en attente…"
                     : "Analyse manuelle : F4 pour vérifier le fichier.");
    } else if (report->backend == LINT_BACKEND_ERROR)
        snprintf(out, cap, "%s", report->status);
    else
        snprintf(out, cap, "%s · %d erreur(s), %d avertissement(s), %d conseil(s)%s",
                 report->backend == LINT_BACKEND_PYLINT
                     ? (strstr(report->status, "Odoo19") ? "PyLint / Odoo19" : "PyLint")
                 : report->backend == LINT_BACKEND_ODOO_XML ? "Odoo19 XML"
                 : report->backend == LINT_BACKEND_XML      ? "XML"
                                                            : "Syntaxe Python",
                 report->errors, report->warnings, report->infos,
                 report->total > report->count ? " · liste limitée à 256" : "");
}
static void problem_rows(App *a, LintReport *report, float y, float bottom) {
    app_lint_quick_bind(a, report);
    float x = (float)a->ex, width = (float)(a->w - a->ex);
    char summary[640];
    diagnostic_summary(a, report, summary, sizeof(summary));
    fit(a, summary, x + 26, y + 52, width - 52, 12,
        report && report->backend == LINT_BACKEND_ERROR ? C_RED : C_MUTED);
    float top = y + 82;
    if (a->has_run && a->last_exit && !a->runner.stopped && top + 24 <= bottom - 8) {
        icon(a, I_WARNING, x + 26, top + 3, 16, C_RED);
        fit(a, "La dernière exécution a échoué. Cliquez pour afficher la sortie.", x + 51, top + 2,
            width - 75, 12, C_RED);
        hit(a, RECT(x + 22, top, width - 44, 26), A_CONSOLE_TAB, 0, "Afficher l’erreur d’exécution Python");
        top += 32;
    }
    if (!report || !report->count) {
        if (top + 17 <= bottom - 8) {
            if (report && report->backend == LINT_BACKEND_SYNTAX && *report->status) {
                char install[BUF_PATH + 80];
                snprintf(install, sizeof(install), "PyLint absent : \"%s\" -m pip install pylint", a->python);
                fit(a, install, x + 26, top, width - 52, 12, C_NUMBER);
            } else if (report && report->backend != LINT_BACKEND_ERROR)
                fit(a,
                    report->backend == LINT_BACKEND_PYLINT ? "Aucun problème détecté par PyLint."
                                                           : "Aucune erreur de syntaxe détectée.",
                    x + 26, top, width - 52, 12, C_ACCENT);
            else if (!report)
                fit(a, "F4 : analyser · F2 / Maj+F2 : diagnostic suivant / précédent", x + 26, top,
                    width - 52, 12, C_DIM);
        }
        return;
    }
    int rows = (int)((bottom - 8 - top) / 32);
    if (rows < 1)
        return;
    int maxscroll = report->count - rows;
    if (maxscroll < 0)
        maxscroll = 0;
    if (a->lint_scroll > maxscroll)
        a->lint_scroll = maxscroll;
    if (a->lint_scroll < 0)
        a->lint_scroll = 0;
    for (int row = 0; row < rows && a->lint_scroll + row < report->count; row++) {
        int index = a->lint_scroll + row;
        Diagnostic *d = &report->items[index];
        float yy = top + (float)row * 32;
        int quick = (report->backend == LINT_BACKEND_PYLINT || report->backend == LINT_BACKEND_ODOO_XML)
                        ? lint_rule_action(d->code, d->symbol)
                        : -1;
        float reserve = quick >= 0 ? 36 : 0;
        Rectangle rect = RECT(x + 20, yy, width - 40 - reserve, 29);
        if (ui_inside(rect, a->ui.mouse) || app_buffer(a)->cursor == d->start)
            ui_box(rect.x, rect.y, rect.width, rect.height, C_RAISED, 4, 230);
        icon(a,
             d->severity == LINT_ERROR     ? I_WARNING
             : d->severity == LINT_WARNING ? I_WARNING
                                           : I_CODE,
             x + 26, yy + 6, 15, diagnostic_tone(d));
        char location[100];
        snprintf(location, sizeof(location), "%zu:%zu  %s", d->row + 1, d->visual_column + 1, d->code);
        ui_text_fit(&a->ui, location, x + 49, yy + 5, 172, 12, diagnostic_tone(d), true);
        fit(a, d->message, x + 230, yy + 5, width - 254 - reserve, 12, C_TEXT);
        hit(a, rect, A_DIAGNOSTIC, a->active_pane * LINT_MAX_ITEMS + index, d->message);
        if (quick >= 0) {
            snprintf(a->lint_quick.tips[index], sizeof(a->lint_quick.tips[index]),
                     odoo_diagnostic_code(d->code) ? "Ignorer %s dans ce projet Odoo"
                                                   : "Ignorer %s pour tous les projets",
                     d->code);
            button(a, RECT(a->w - 49, yy + 2, 26, 25), A_LINT_IGNORE, quick, NULL, I_MINUS, 1, false,
                   a->lint_quick.tips[index]);
        }
    }
}

static void editor(App *a, float dt) {
    Buffer *b = app_buffer(a);
    LintReport *diagnostics = app_diagnostics(a);
    BOX(a->pane_x, a->code_y, a->pane_right - a->pane_x, a->code_h, C_RAIL);
    BeginScissorMode(a->pane_x + 1, a->code_y, a->pane_right - a->pane_x - 1, a->code_h);
    if (a->effects) {
        DrawCircleGradient(a->pane_right - 200 + (int)(sinf((float)GetTime() * 0.22f) * 35),
                           a->code_y + (int)((float)a->code_h * 0.63f), 310, ui_alpha(C_ACCENT, 10),
                           ui_alpha(C_ACCENT, 0));
        int span = a->pane_right - a->pane_x - 270;
        if (span < 1)
            span = 1;
        for (int i = 0; i < 16; i++) {
            float x = (float)(a->pane_x + 250 + (i * 197) % span),
                  y = (float)a->code_y +
                      fmodf((float)(i * 97) + sinf((float)GetTime() * 0.3f + (float)i) * 5 + (float)a->code_h,
                            (float)a->code_h);
            DrawCircle((int)x, (int)y, 0.8f, ui_alpha(C_ACCENT, 17));
        }
    }
    int first = (int)(b->scroll_y / a->line_h) - 1;
    if (first < 0)
        first = 0;
    int last = first + (int)((float)a->code_h / a->line_h) + 4;
    if ((size_t)last > b->line_count)
        last = (int)b->line_count;
    size_t active = buf_row(b, b->cursor);
    float active_y = (float)a->code_y + 8 + (float)active * a->line_h - b->scroll_y;
    ui_box((float)a->pane_x + 12, active_y, (float)(a->code_right - a->pane_x - 10), a->line_h, C_ACCENT, 3,
           7);
    ui_box((float)a->pane_x + 13, active_y + 3, 2, a->line_h - 6, C_ACCENT, 1, 150);
    if (a->find_open && a->render_pane == a->active_pane)
        for (int i = 0; i < a->match_count; i++)
            if (buf_row(b, a->matches[i].start) >= (size_t)first &&
                buf_row(b, a->matches[i].start) < (size_t)last)
                range_draw(a, a->matches[i].start, a->matches[i].end, C_NUMBER, 32);
    for (int i = 0; i < buf_caret_count(b); i++) {
        BufCaret c = buf_caret(b, i);
        if (c.anchor != BUF_NO_SELECTION && c.anchor != c.cursor)
            range_draw(a, c.anchor < c.cursor ? c.anchor : c.cursor,
                       c.anchor > c.cursor ? c.anchor : c.cursor, C_BLUE, 53);
    }
    for (int row = first; row < last; row++) {
        float y = (float)a->code_y + 13 + (float)row * a->line_h - b->scroll_y;
        char n[32];
        snprintf(n, sizeof(n), "%d", row + 1);
        mono(a, n, (float)a->pane_x + 50 - ui_width(&a->ui, n, 11, true), y + 3, 11,
             (size_t)row == active ? C_ACCENT : C_DIM);
    }
    diagnostic_gutter(a, diagnostics, first, last);
    EndScissorMode();
    BeginScissorMode(a->code_x - 2, a->code_y, a->code_right - a->code_x, a->code_h);
    for (int row = first; row < last; row++)
        code_line(a, (size_t)row, (float)a->code_y + 13 + (float)row * a->line_h - b->scroll_y);
    diagnostic_underlines(a, diagnostics, first, last);
    if (a->focus == 0 && !a->modal && a->render_pane == a->active_pane) {
        Vector2 c = app_caret(a, b->cursor);
        if (a->caret_x < 0 || !a->effects || fabsf(c.y - a->caret_y) > a->line_h * 3) {
            a->caret_x = c.x;
            a->caret_y = c.y;
        } else {
            float k = 1 - expf(-dt * 32);
            a->caret_x += (c.x - a->caret_x) * k;
            a->caret_y += (c.y - a->caret_y) * k;
        }
        if (GetTime() - a->caret_time < 0.8 || ((int)((GetTime() - a->caret_time) * 1.8) % 2 == 0)) {
            if (a->effects) {
                ui_box(a->caret_x - 5, a->caret_y - 1, 12, (float)a->font_size + 6, C_ACCENT, 4, 12);
                ui_box(a->caret_x - 2, a->caret_y, 5, (float)a->font_size + 4, C_ACCENT, 2, 22);
            }
            BOX(a->caret_x, a->caret_y, 1.5f, a->font_size + 3, C_ACCENT);
            for (int i = 1; i < buf_caret_count(b); i++) {
                Vector2 extra = app_caret(a, buf_caret(b, i).cursor);
                BOX(extra.x, extra.y, 1.8f, a->font_size + 3, C_ACCENT);
            }
        }
    }
    if (a->render_pane != a->active_pane) {
        Vector2 inactive = app_caret(a, b->cursor);
        ui_box(inactive.x, inactive.y, 1, a->font_size + 3, C_DIM, 0, 130);
    }
    for (int i = 0; a->render_pane == a->active_pane && i < a->particle_count; i++) {
        Particle *p = &a->particles[i];
        DrawCircle((int)p->x, (int)p->y, fmaxf(0.5f, p->life * 3),
                   ui_alpha(C_ACCENT, (int)fminf(200, p->life * 420)));
    }
    EndScissorMode();
    ui_line((float)a->pane_x + 63, (float)a->code_y + 10, (float)a->pane_x + 63, (float)a->code_bottom - 10,
            C_BORDER, 1, 80);
    if (a->minimap_w > 20)
        minimap(a);
    float content = (float)b->line_count * a->line_h + a->line_h * 3;
    if (content > a->code_h) {
        float h = fmaxf(25, (float)a->code_h * (float)a->code_h / content),
              y = (float)a->code_y + b->scroll_y / content * (float)a->code_h;
        ui_box((float)a->pane_right - 7, y + 4, 3, fminf(h, (float)a->code_bottom - y - 4), C_DIM, 1, 120);
    }
}
static void console(App *a) {
    float x = (float)a->ex, y = a->terminal_y, bottom = (float)a->layout.status_y;
    BOX(x, y, a->w - a->ex, bottom - y, C_BG);
    ui_line(x, y, (float)a->w, y, C_BORDER, 1, 255);
    const char *labels[] = {"SORTIE", "ERREURS", "RACCOURCIS"};
    LintReport *diagnostics = app_diagnostics(a);
    float widths[] = {90, 130, 130};
    float tx = x + 22;
    for (int i = 0; i < 3; i++) {
        label(a, labels[i], tx, y + 14, 8, a->console_mode == i ? C_TEXT : C_DIM);
        if (i == 1) {
            bool runtime_error = a->has_run && a->last_exit && !a->runner.stopped;
            int count = (diagnostics ? diagnostics->count : 0) + (runtime_error ? 1 : 0);
            char badge[16];
            if (!diagnostics && python(app_buffer(a)))
                snprintf(badge, sizeof(badge), "…");
            else
                snprintf(badge, sizeof(badge), count > 99 ? "99+" : "%d", count);
            bool errors = (diagnostics && diagnostics->errors) || runtime_error;
            ui_box(tx + 75, y + 10, 38, 24, errors ? C_RED : C_RAISED, 4, errors ? 30 : 255);
            text(a, badge, tx + 80, y + 13, 8, errors ? C_RED : C_MUTED);
        }
        if (a->console_mode == i)
            ui_box(tx, y + 38, widths[i] - 17, 2, C_ACCENT, 1, 255);
        hit(a, RECT(tx - 3, y + 3, widths[i], 37), A_CONSOLE_TAB, i, NULL);
        tx += widths[i] + 13;
    }
    if (a->w - a->ex > 900) {
        DrawCircle(a->w - 301, (int)(y + 21), 2.5f, ui_colors[a->runner.running ? C_ACCENT : C_DIM]);
        text(a, a->runner.running ? "Python 3 · actif" : "Python 3 · prêt", (float)a->w - 291, y + 13, 10,
             C_MUTED);
    }
    if (a->w - a->ex >= 650)
        button(a, RECT(a->w - 151, y + 7, 27, 27), A_LINT_RUN, 0, NULL, I_CODE, 0, false,
               "Analyser le fichier avec PyLint · F4");
    button(a, RECT(a->w - 116, y + 7, 27, 27), A_CONSOLE_CLEAR, 0, NULL, I_TRASH, 0, false,
           "Effacer la sortie");
    button(a, RECT(a->w - 81, y + 7, 27, 27), A_CONSOLE_EXPAND, 0, NULL, I_EXPAND, 0, false,
           "Agrandir / réduire la console");
    button(a, RECT(a->w - 45, y + 7, 27, 27), A_TERMINAL, 0, NULL, I_DOWN, 0, false,
           "Replier la console · Ctrl+J");
    float top = y + 50, content_bottom = bottom - (a->runner.running ? 36 : 8);
    BeginScissorMode(a->ex + 20, (int)(y + 46), a->w - a->ex - 40, (int)fmaxf(1, bottom - y - 46));
    if (a->console_mode == 1) {
        problem_rows(a, diagnostics, y, bottom);
    } else if (a->console_mode == 2) {
        const char *keys[] = {"F5", "Ctrl S", "Ctrl P", "F6", "Ctrl Espace"};
        const char *desc[] = {"Exécuter le fichier", "Enregistrer", "Ouvrir un fichier rapidement",
                              "Activer / désactiver les effets", "Complétion Python / Odoo et macros"};
        for (int i = 0; i < 5; i++) {
            mono(a, keys[i], x + 28, y + 53 + (float)i * 27, 11, C_ACCENT);
            fit(a, desc[i], x + 180, y + 53 + (float)i * 27, a->w - a->ex - 204, 11, C_MUTED);
        }
    } else {
        int entries = a->console_count + (a->output_length ? 1 : 0);
        int rows = (int)((content_bottom - top) / 25);
        if (rows < 1)
            rows = 1;
        int end = entries - a->console_scroll;
        if (end < 0)
            end = 0;
        int start = end - rows;
        if (start < 0)
            start = 0;
        for (int i = start; i < end; i++) {
            const char *line = i < a->console_count ? a->console[i].text : a->output_partial;
            Tone tone = i < a->console_count ? a->console[i].tone : C_TEXT;
            mono(a, line, x + 27, top + (float)(i - start) * 25, 17, tone);
        }
        if (a->runner.running) {
            float iy = bottom - 33;
            mono(a, "›", x + 25, iy + 6, 17, C_ACCENT);
            app_field_draw(a, &a->input, RECT(x + 46, iy, a->w - a->ex - 67, 30), a->focus == 2,
                           "Réponse à input()…", A_FIELD_CONSOLE, true);
        }
    }
    EndScissorMode();
}
static void status(App *a) {
    float y = (float)a->layout.status_y;
    /* Fill exactly to the current window edge, never to a cached/inset height. */
    BOX(0, y, a->w, a->layout.status_h, C_STATUS);
    ui_line(0, y, (float)a->w, y, C_BORDER, 1, 255);
    button(a, RECT(8, y + 4, 100, 28), A_SIDEBAR, 0, "Dossiers", I_FILES, 0, a->sidebar,
           "Afficher / replier l’explorateur · Ctrl+B");
    button(a, RECT(112, y + 4, 80, 28), A_TOPBAR, 0, "Menu", I_UP, 0, a->topbar,
           "Afficher / replier le menu du haut · F8");
    button(a, RECT(196, y + 4, 96, 28), A_TERMINAL, 0, "Console", I_TERMINAL, 0, a->terminal,
           "Afficher / replier la console · Ctrl+J");
    button(a, RECT(296, y + 4, 130, 28), A_ZEN, 0, a->zen_mode ? "Restaurer" : "Tout replier", I_CODE, 0,
           a->zen_mode, "Tout replier / restaurer les panneaux précédents · F9");
    button(a, RECT(432, y + 4, 26, 28), A_SETTINGS, 0, NULL, I_GEAR, 0, false, "Paramètres · Ctrl+,");
    button(a, RECT(462, y + 4, 26, 28), A_HELP, 0, NULL, I_BOOK, 0, false, "Raccourcis · F1");
    button(a, RECT(492, y + 4, 73, 28), A_SSH, 0,
           a->ssh.state == SSH_MOUNTED      ? "SSH ✓"
           : a->ssh.state == SSH_CONNECTING ? "SSH…"
           : a->ssh.state == SSH_LOST       ? "SSH !"
                                            : "SSH",
           I_FOLDER, 0, a->ssh.state == SSH_MOUNTED,
           "Connexions SSH mémorisées · fichiers distants, Python local");
    float ty = y + (a->layout.status_h - ui_text_size(12)) / 2 - 1, right = (float)a->w - 18;
    char fps[32];
    snprintf(fps, sizeof(fps), "%d fps", GetFPS());
    float width = 0;
    if (a->w >= 1440) {
        width = ui_width(&a->ui, fps, 12, true);
        right -= width;
        mono(a, fps, right, ty, 12, a->effects ? C_ACCENT : C_DIM);
        right -= 16;
    }
    const char *fields[] = {"Python 3", "UTF-8", "4 espaces"};
    for (int i = 0; i < 3; i++) {
        if ((i == 0 && a->w < 1200) || (i == 1 && a->w < 1600) || (i == 2 && a->w < 1760))
            continue;
        width = ui_width(&a->ui, fields[i], 12, false);
        right -= width;
        text(a, fields[i], right, ty, 12, C_MUTED);
        right -= 16;
    }
    Buffer *b = app_buffer(a);
    char position[100];
    if (b->extra_count)
        snprintf(position, sizeof(position), "%d curseurs", buf_caret_count(b));
    else
        snprintf(position, sizeof(position), "Ln %zu, Col %zu", buf_row(b, b->cursor) + 1,
                 buf_column(b, b->cursor) + 1);
    width = fminf(ui_width(&a->ui, position, 12, true), fmaxf(0, right - 604));
    right -= width;
    ui_text_fit(&a->ui, position, right, ty, width, 12, C_TEXT, true);
    ui_line(right - 12, y + 8, right - 12, y + a->layout.status_h - 8, C_BORDER, 1, 255);
    char analysis[180] = "";
    LintReport *report = app_diagnostics(a);
    if (python(b)) {
        LintDocument *doc = a->analyses[a->active];
        if (a->linter.process.running && a->linter.owner == doc)
            snprintf(analysis, sizeof(analysis), "Analyse Python…");
        else if (report && report->backend == LINT_BACKEND_PYLINT)
            snprintf(analysis, sizeof(analysis), "PyLint · %d E / %d A", report->errors, report->warnings);
        else if (report && report->backend == LINT_BACKEND_SYNTAX)
            snprintf(analysis, sizeof(analysis), "%s", report->count ? "Erreur de syntaxe" : "Syntaxe seule");
        else if (report && report->backend == LINT_BACKEND_ERROR)
            snprintf(analysis, sizeof(analysis), "Analyse indisponible");
        else
            snprintf(analysis, sizeof(analysis), "%s",
                     a->lint_auto ? "Analyse en attente…" : "Analyse manuelle · F4");
    }
    fit(a,
        a->runner.running ? "Python en cours…"
        : *analysis       ? analysis
        : a->zen_mode     ? "Concentration"
        : a->split        ? (a->active_pane ? "2 volets · droite" : "2 volets · gauche")
                          : "C11 + raylib",
        578, ty, right - 604, 12, C_DIM);
}
static void completions(App *a) {
    Vector2 c = app_caret(a, app_buffer(a)->cursor);
    int rows = a->completion_count < 7 ? a->completion_count : 7;
    float w = fminf(470, (float)a->w - 24), h = rows ? (float)rows * 34 + 42 : 69;
    float x = fmaxf(12, fminf(c.x, (float)a->w - w - 12));
    float y = c.y + a->line_h + 3;
    if (y + h > (float)a->layout.status_y - 8)
        y = c.y - h - 5;
    y = fmaxf(8, fminf(y, (float)a->h - h - 12));
    a->completion_rect = RECT(x, y, w, h);
    ui_box(x + 4, y + 5, w, h, C_BLACK, 7, 55);
    ui_box(x, y, w, h, C_RAISED, 7, 255);
    ui_outline(x, y, w, h, C_DIM, 7, 130);
    if (!rows) {
        fit(a, a->path_completion.active ? "Lecture du dossier…" : "Recherche dans les sources du framework…",
            x + 14, y + 13, w - 28, 12, C_MUTED);
        fit(a, "Échap : fermer · aucun code exécuté", x + 14, y + 40, w - 28, 10, C_DIM);
        return;
    }
    int first = a->completion_index >= rows ? a->completion_index - rows + 1 : 0;
    for (int row = 0; row < rows && first + row < a->completion_count; row++) {
        int i = first + row;
        CompletionItem *item = &a->completions[i];
        float yy = y + 6 + (float)row * 34;
        if (i == a->completion_index)
            ui_box(x + 5, yy, w - 10, 32, C_ACCENT, 3, 16);
        icon(a,
             item->kind == 'D'   ? I_FOLDER
             : item->kind == 'P' ? I_FILE
                                 : I_CODE,
             x + 13, yy + 9, 14, item->kind == 'S' ? C_ACCENT : C_LAVENDER);
        ui_text_fit(&a->ui, item->name, x + 39, yy + 8, w - 144, 12, C_TEXT, true);
        const char *kind = item->kind == 'D'   ? "Dossier"
                           : item->kind == 'P' ? "Fichier"
                           : item->kind == 'S' ? "Macro"
                           : item->kind == 'C' ? "Classe"
                           : item->kind == 'M' ? "Module"
                           : item->kind == 'O' ? "Modèle"
                           : item->kind == 'F' ? "API"
                           : item->kind == 'W' ? "Mot"
                                               : "Symbole";
        label(a, kind, x + w - 93, yy + 10, 8, item->kind == 'S' ? C_ACCENT : C_MUTED);
        hit(a, RECT(x + 5, yy, w - 10, 34), A_COMPLETION, i, item->detail);
    }
    fit(a, "↑↓ · Tab / Entrée", x + 13, y + h - 27, 180, 10, C_DIM);
    fit(a, *a->completion_status ? a->completion_status : a->completions[a->completion_index].detail, x + 198,
        y + h - 27, w - 211, 10, C_MUTED);
    if (*a->completion_status)
        hit(a, RECT(x + 198, y + h - 30, w - 211, 26), A_NONE, 0, a->completion_status);
}
static Rectangle shell(App *a, const char *title, const char *subtitle, float w, float h) {
    ui_box(0, 0, (float)a->w, (float)a->h, C_BG, 0, 220);
    float x = ((float)a->w - w) / 2;
    float y = fmaxf(12, fminf(fmaxf(70, ((float)a->h - h) / 2 - 30), (float)a->h - h - 12));
    ui_box(x + 7, y + 14, w, h, C_BLACK, 12, 80);
    ui_box(x, y, w, h, C_PANEL, 12, 255);
    ui_outline(x, y, w, h, C_DIM, 12, 100);
    ui_box(x + 25, y, w - 50, 1, C_ACCENT, 0, 80);
    text(a, title, x + 28, y + 26, 19, C_WHITE);
    if (subtitle)
        fit(a, subtitle, x + 29, y + 57, w - 58, 11, C_MUTED);
    if (a->modal != M_FS_BUSY)
        button(a, RECT(x + w - 52, y + 20, 30, 30), A_MODAL_CLOSE, 0, NULL, I_CLOSE, 0, false,
               "Fermer · Échap");
    return RECT(x, y, w, h);
}
Rectangle app_ssh_shell(App *a, const char *title, const char *subtitle, float w, float h) {
    return shell(a, title, subtitle, w, h);
}
static void palette(App *a) {
    app_filter_palette(a);
    bool files = a->browser_mode != 0;
    int count = a->filtered_count < 8 ? a->filtered_count : 8;
    if (!count)
        count = 1;
    Rectangle r = shell(a, files ? "Ouvrir rapidement" : "Votre prochaine action",
                        files ? "Recherchez un fichier de l'atelier." : "Moins de clics. Plus de création.",
                        650, 161 + (float)count * 40);
    app_field_draw(a, &a->field, RECT(r.x + 25, r.y + 88, r.width - 50, 38), true,
                   files ? "Nom du fichier…" : "Que souhaitez-vous faire ?", A_NONE, false);
    int start = a->palette_index - 7;
    if (start < 0)
        start = 0;
    for (int j = 0; j < 8 && start + j < a->filtered_count; j++) {
        int index = a->filtered[start + j];
        PaletteItem *p = &a->palette[index];
        float y = r.y + 139 + (float)j * 40;
        bool selected = start + j == a->palette_index;
        if (selected)
            ui_box(r.x + 13, y, r.width - 26, 37, C_ACCENT, 5, 12);
        icon(a, p->icon, r.x + 28, y + 10, 16, selected ? C_ACCENT : C_MUTED);
        float hint_w = ui_width(&a->ui, p->hint, 10, true);
        fit(a, p->text, r.x + 59, y + 10, r.width - 104 - hint_w, 12, selected ? C_WHITE : C_TEXT);
        mono(a, p->hint, r.x + r.width - ui_width(&a->ui, p->hint, 10, true) - 29, y + 13, 10, C_DIM);
        hit(a, RECT(r.x + 13, y, r.width - 26, 37), A_PALETTE_CHOOSE, index, NULL);
    }
    if (!a->filtered_count)
        text(a, "Aucun résultat. Essayez un autre mot.", r.x + 30, r.y + 151, 12, C_MUTED);
}
static void browser(App *a) {
    const char *titles[] = {"Ouvrir un fichier",
                            "Ouvrir un atelier",
                            "Enregistrer votre fichier",
                            "Ajouter un dossier d’import PyLint",
                            "Ajouter un dossier de recherche XML",
                            "Choisir le dossier de recherche de texte"};
    Rectangle r = shell(a, titles[a->browser_mode], "", 730, 511);
    fit(a,
        *a->browser_completion.status ? a->browser_completion.status
                                      : "Ctrl+Espace : suggérer le contenu du chemin",
        r.x + 29, r.y + 57, r.width - 90, 11, C_MUTED);
    app_field_draw(a, &a->field, RECT(r.x + 27, r.y + 87, r.width - 99, 37), true,
                   a->browser_mode == 1 || a->browser_mode == 3 || a->browser_mode == 4 ||
                           a->browser_mode == 5
                       ? "Chemin du dossier…"
                       : "Chemin du fichier…",
                   A_NONE, false);
    button(a, RECT(r.x + r.width - 61, r.y + 88, 33, 34), A_BROWSER_UP, 0, NULL, I_UP, 1, false,
           "Dossier parent");
    label(a, "NOM", r.x + 41, r.y + 149, 9, C_MUTED);
    label(a, "TYPE", r.x + r.width - 112, r.y + 149, 9, C_MUTED);
    ui_line(r.x + 27, r.y + 172, r.x + r.width - 27, r.y + 172, C_BORDER, 1, 255);
    for (int j = 0; j < 7 && a->browser_scroll + j < a->browser_count; j++) {
        int i = a->browser_scroll + j;
        Entry *e = &a->browser[i];
        float y = r.y + 179 + (float)j * 35;
        if (ui_inside(RECT(r.x + 27, y, r.width - 54, 34), a->ui.mouse) || !strcmp(a->field.text, e->path))
            ui_box(r.x + 23, y, r.width - 46, 33, C_RAISED, 4, 255);
        icon(a,
             e->directory                      ? I_FOLDER
             : IsFileExtension(e->name, ".py") ? I_PYTHON
                                               : I_FILE,
             r.x + 39, y + 8, 17, e->directory ? C_ACCENT : C_MUTED);
        BeginScissorMode((int)(r.x + 68), (int)y, (int)(r.width - 215), 34);
        fit(a, e->name, r.x + 70, y + 9, r.width - 218, 12, C_TEXT);
        EndScissorMode();
        fit(a,
            e->directory                ? "Dossier"
            : GetFileExtension(e->name) ? GetFileExtension(e->name)
                                        : "Fichier",
            r.x + r.width - 112, y + 10, 84, 10, C_DIM);
        hit(a, RECT(r.x + 27, y, r.width - 54, 34), A_BROWSER_ENTRY, i, NULL);
    }
    if (!a->browser_count)
        text(a, "Ce dossier est vide.", r.x + 42, r.y + 200, 12, C_DIM);
    ui_line(r.x + 27, r.y + r.height - 71, r.x + r.width - 27, r.y + r.height - 71, C_BORDER, 1, 255);
    text(a, "Entrée pour valider · Échap pour annuler", r.x + 28, r.y + r.height - 40, 10, C_DIM);
    button(a, RECT(r.x + r.width - 202, r.y + r.height - 54, 174, 33), A_BROWSER_SUBMIT, 0,
           a->browser_mode == 2                           ? "Enregistrer"
           : a->browser_mode == 3 || a->browser_mode == 4 ? "Ajouter ce dossier"
           : a->browser_mode == 1 || a->browser_mode == 5 ? "Choisir ce dossier"
                                                          : "Ouvrir",
           -1, 2, false, NULL);
}
static void toggle(App *a, float x, float y, const char *title, const char *subtitle, bool active,
                   Action id) {
    fit(a, title, x, y, 420, 13, C_TEXT);
    fit(a, subtitle, x, y + 24, 420, 10, C_MUTED);
    ui_box(x + 439, y + 4, 36, 20, active ? C_ACCENT : C_DIM, 10, active ? 255 : 140);
    DrawCircle((int)(x + (active ? 465 : 449)), (int)(y + 14), 7, ui_colors[active ? C_STATUS : C_TEXT]);
    hit(a, RECT(x, y - 4, 480, 55), id, 0, NULL);
}
static void settings(App *a) {
    Rectangle r = shell(a, "Paramètres", "Préférences globales", 590, 572);
    toggle(a, r.x + 29, r.y + 100, "Effets visuels", "Halo, curseur fluide et particules à la frappe.",
           a->effects, A_EFFECTS);
    ui_line(r.x + 29, r.y + 161, r.x + r.width - 29, r.y + 161, C_BORDER, 1, 255);
    toggle(a, r.x + 29, r.y + 174, "Minimap du code", "Gardez une vue d’ensemble du fichier.", a->minimap,
           A_MINIMAP_TOGGLE);
    ui_line(r.x + 29, r.y + 235, r.x + r.width - 29, r.y + 235, C_BORDER, 1, 255);
    fit(a, "Analyse Python automatique", r.x + 29, r.y + 248, 265, 13, C_TEXT);
    fit(a, "PyLint + syntaxe · analyse manuelle : F4.", r.x + 29, r.y + 286, r.width - 58, 10, C_MUTED);
    ui_box(r.x + 468, r.y + 252, 36, 20, a->lint_auto ? C_ACCENT : C_DIM, 10, a->lint_auto ? 255 : 140);
    DrawCircle((int)(r.x + (a->lint_auto ? 494 : 478)), (int)(r.y + 262), 7,
               ui_colors[a->lint_auto ? C_STATUS : C_TEXT]);
    hit(a, RECT(r.x + 29, r.y + 244, 265, 36), A_LINT_AUTO, 0, NULL);
    hit(a, RECT(r.x + 444, r.y + 244, 88, 36), A_LINT_AUTO, 0, NULL);
    button(a, RECT(r.x + 318, r.y + 244, 110, 32), A_LINT_RULES, 0, "Règles…", -1, 1, false,
           "Configurer les messages PyLint à ignorer");
    ui_line(r.x + 29, r.y + 309, r.x + r.width - 29, r.y + 309, C_BORDER, 1, 255);
    mono(a, "JetBrains Mono Medium", r.x + 29, r.y + 342, 13, C_WHITE);
    button(a, RECT(r.x + r.width - 161, r.y + 331, 31, 31), A_FONT_DOWN, 0, NULL, I_MINUS, 1, false, NULL);
    char size[20];
    snprintf(size, sizeof(size), "%d", a->font_size);
    mono(a, size, r.x + r.width - 113, r.y + 344, 12, C_ACCENT);
    button(a, RECT(r.x + r.width - 70, r.y + 331, 31, 31), A_FONT_UP, 0, NULL, I_PLUS, 1, false, NULL);
    fit(a, "Imports Python / complétion", r.x + 29, r.y + 390, 300, 13, C_TEXT);
    char imports[40];
    snprintf(imports, sizeof(imports), "Gérer (%d)", a->linter.imports.count);
    button(a, RECT(r.x + r.width - 190, r.y + 380, 160, 34), A_IMPORTS, 0, imports, I_FOLDER, 1, false,
           "Configurer les imports supplémentaires pour le projet courant");
    fit(a, "Macros / extraits de code", r.x + 29, r.y + 437, 300, 13, C_TEXT);
    char macros[40];
    snprintf(macros, sizeof(macros), "Gérer (%d)", a->snippets.count);
    button(a, RECT(r.x + r.width - 190, r.y + 427, 160, 34), A_SNIPPETS, 0, macros, I_CODE, 1, false,
           "Créer et modifier la bibliothèque commune");
    fit(a, "Dossiers XML / balises auto", r.x + 29, r.y + 484, 300, 13, C_TEXT);
    char xml[40];
    snprintf(xml, sizeof(xml), "Gérer (%d)", a->xml_paths.count);
    button(a, RECT(r.x + r.width - 190, r.y + 474, 160, 34), A_XML_PATHS, 0, xml, I_CODE, 1, false,
           "Racines des chemins XML relatifs et balises automatiques");
    button(a, RECT(r.x + 29, r.y + 522, r.width - 58, 30), A_ODOO_SETTINGS, 0,
           "Odoo 19 — Python / XML — Ce projet", I_CODE, 1, false,
           "Contrôles Odoo19 et règles ignorées propres au projet");
}
static void lint_rules_settings(App *a) {
    char subtitle[128];
    snprintf(subtitle, sizeof(subtitle), "Préférences globales · %u règle(s) ignorée(s)",
             a->linter.disabled.count);
    Rectangle r = shell(a, "Règles PyLint à ignorer", subtitle, 700, 350);
    fit(a, "IDs ou noms, séparés par des virgules, espaces ou points-virgules.", r.x + 28, r.y + 94,
        r.width - 56, 12, C_TEXT);
    app_field_draw(a, &a->field, RECT(r.x + 28, r.y + 120, r.width - 56, 42), true,
                   "C0114, W0611, missing-function-docstring", A_NONE, false);
    fit(a, "Exemple : C0114, W0611, missing-function-docstring", r.x + 28, r.y + 177, r.width - 56, 12,
        C_MUTED);
    fit(a, "Vide = aucune règle ignorée. La syntaxe reste toujours vérifiée.", r.x + 28, r.y + 200,
        r.width - 56, 12, C_MUTED);
    fit(a,
        *a->lint_rules_error ? a->lint_rules_error
                             : "64 règles maximum · Entrée : appliquer · Échap : annuler",
        r.x + 28, r.y + 233, r.width - 56, 12, *a->lint_rules_error ? C_RED : C_MUTED);
    if (*a->lint_rules_error)
        hit(a, RECT(r.x + 28, r.y + 224, r.width - 56, 28), A_NONE, 0, a->lint_rules_error);
    button(a, RECT(r.x + 28, r.y + 289, 153, 34), A_LINT_RULES_CLEAR, 0, "Tout réactiver", -1, 1, false,
           "Vider le brouillon ; appliquer pour réactiver les messages");
    button(a, RECT(r.x + r.width - 265, r.y + 289, 105, 34), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
    button(a, RECT(r.x + r.width - 148, r.y + 289, 120, 34), A_LINT_RULES_APPLY, 0, "Appliquer", -1, 2, false,
           NULL);
}
static void odoo_settings(App *a) {
    Rectangle r =
        shell(a, "Vérification Odoo 19",
              "Python : vrai pylint-odoo · XML : contrôles statiques · Ce projet seulement", 730, 420);
    fit(a, "Contrôles Odoo19 Python et XML", r.x + 28, r.y + 101, r.width - 220, 14, C_TEXT);
    button(a, RECT(r.x + r.width - 150, r.y + 90, 122, 34), A_ODOO_TOGGLE, 0,
           a->odoo_draft_enabled ? "Activés" : "Désactivés", -1, 1, a->odoo_draft_enabled, NULL);
    fit(a, "Règles Odoo ignorées dans ce projet (ID ou noms)", r.x + 28, r.y + 140, r.width - 56, 12,
        C_MUTED);
    app_field_draw(a, &a->field, RECT(r.x + 28, r.y + 162, r.width - 56, 42), true,
                   "W8165, odoo-xml-duplicate-id", A_NONE, false);
    fit(a, "Python : installez pylint-odoo dans l’interpréteur sélectionné ; aucun serveur Odoo lancé.",
        r.x + 28, r.y + 219, r.width - 56, 12, C_MUTED);
    fit(a, "XML : syntaxe toujours vérifiée ; références locales ; arch fermé vide accepté.", r.x + 28,
        r.y + 244, r.width - 56, 12, C_MUTED);
    fit(a, *a->odoo_error ? a->odoo_error : "64 règles / 4 095 octets · Entrée : appliquer · Échap : annuler",
        r.x + 28, r.y + 280, r.width - 56, 12, *a->odoo_error ? C_RED : C_MUTED);
    if (*a->odoo_error)
        hit(a, RECT(r.x + 28, r.y + 271, r.width - 56, 28), A_NONE, 0, a->odoo_error);
    button(a, RECT(r.x + 28, r.y + 355, 153, 34), A_ODOO_CLEAR, 0, "Tout réactiver", -1, 1, false, NULL);
    button(a, RECT(r.x + r.width - 265, r.y + 355, 105, 34), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
    button(a, RECT(r.x + r.width - 148, r.y + 355, 120, 34), A_ODOO_APPLY, 0, "Appliquer", -1, 2, false,
           NULL);
}
static void imports_settings(App *a) {
    Rectangle r = shell(a, "Dossiers d’import Python", a->project_name, 720, 552);
    fit(a, "Choisissez le dossier parent du paquet de votre framework.", r.x + 28, r.y + 89, r.width - 56, 13,
        C_TEXT);
    fit(a, "Ex. /sdk/python pour /sdk/python/mon_framework/__init__.py", r.x + 28, r.y + 115, r.width - 56,
        11, C_MUTED);
    ui_line(r.x + 28, r.y + 143, r.x + r.width - 28, r.y + 143, C_BORDER, 1, 255);
    if (!a->linter.imports.count)
        fit(a, "Aucun dossier supplémentaire pour ce projet.", r.x + 29, r.y + 190, r.width - 58, 13, C_DIM);
    for (int i = 0; i < a->linter.imports.count; i++) {
        float y = r.y + 160 + (float)i * 34;
        ui_box(r.x + 25, y, r.width - 50, 32, C_RAISED, 4, 255);
        icon(a, I_FOLDER, r.x + 34, y + 8, 17, C_ACCENT);
        fit(a, a->linter.imports.paths[i], r.x + 62, y + 8, r.width - 137, 12, C_TEXT);
        hit(a, RECT(r.x + 25, y, r.width - 91, 32), A_NONE, 0, a->linter.imports.paths[i]);
        button(a, RECT(r.x + r.width - 65, y + 1, 32, 30), A_IMPORT_REMOVE, i, NULL, I_TRASH, 0, false,
               "Retirer ce chemin de l’analyse / complétion (aucun fichier supprimé)");
    }
    fit(a, *a->import_error ? a->import_error : "Par projet · PyLint + complétion statique · F5 inchangé.",
        r.x + 28, r.y + 447, r.width - 56, 11, *a->import_error ? C_RED : C_MUTED);
    if (a->linter.imports.count < IMPORT_PATHS_MAX)
        button(a, RECT(r.x + 28, r.y + 493, 238, 34), A_IMPORT_ADD, 0, "Ajouter un dossier", I_PLUS, 2, false,
               NULL);
    else
        fit(a, "Huit dossiers configurés.", r.x + 29, r.y + 504, 300, 12, C_DIM);
    button(a, RECT(r.x + 282, r.y + 493, 140, 34), A_COMPLETION_REFRESH, 0, "Actualiser", I_CODE, 1, false,
           "Vider le cache de complétion ; réindexation à la prochaine saisie / Ctrl+Espace");
    button(a, RECT(r.x + r.width - 231, r.y + 493, 203, 34), A_SETTINGS, 0, "Retour aux paramètres", -1, 1,
           false, NULL);
}
static void xml_paths_settings(App *a) {
    Rectangle r = shell(a, "Dossiers de recherche XML", a->project, 820, 570);
    fit(a, "Balises XML automatiques", r.x + 28, r.y + 97, 430, 13, C_TEXT);
    button(a, RECT(r.x + r.width - 143, r.y + 88, 114, 32), A_XML_AUTO, 0,
           a->xml_auto ? "Activé" : "Désactivé", -1, 1, a->xml_auto,
           "Fermeture sur > et </, indentation. Ctrl+Espace reste disponible.");
    fit(a, "Chemins relatifs : dossiers ci-dessous, puis projet, document, imports.", r.x + 28, r.y + 132,
        r.width - 56, 11, C_MUTED);
    for (int i = 0; i < a->xml_paths.count; i++) {
        float y = r.y + 157 + (float)i * 35;
        ui_box(r.x + 23, y, r.width - 46, 32, C_RAISED, 4, 255);
        char index[16];
        snprintf(index, sizeof(index), "%d", i + 1);
        text(a, index, r.x + 33, y + 9, 12, C_ACCENT);
        char display[BUF_PATH];
        snprintf(display, sizeof(display), "%s", a->xml_paths.paths[i]);
        for (size_t j = 0; display[j]; j++)
            if (display[j] == '\r' || display[j] == '\n' || display[j] == '\t')
                display[j] = ' ';
        fit(a, display, r.x + 60, y + 9, r.width - 208, 12, C_TEXT);
        if (i)
            button(a, RECT(r.x + r.width - 136, y + 1, 30, 30), A_XML_PATH_UP, i, NULL, I_UP, 0, false,
                   "Chercher ce dossier avant le précédent");
        if (i + 1 < a->xml_paths.count)
            button(a, RECT(r.x + r.width - 103, y + 1, 30, 30), A_XML_PATH_DOWN, i, NULL, I_DOWN, 0, false,
                   "Chercher ce dossier après le suivant");
        button(a, RECT(r.x + r.width - 70, y + 1, 30, 30), A_XML_PATH_REMOVE, i, NULL, I_TRASH, 0, false,
               "Retirer ce chemin sans supprimer le dossier");
    }
    if (!a->xml_paths.count)
        fit(a, "Ajoutez par exemple /opt/odoo pour saisir addons/sale/views/…xml.", r.x + 29, r.y + 179,
            r.width - 58, 12, C_MUTED);
    if (*a->xml_paths_error)
        fit(a, a->xml_paths_error, r.x + 29, r.y + 448, r.width - 58, 11, C_RED);
    fit(a, "Par projet, sans scan récursif ni effet sur Python / F5. Maximum : 8 dossiers.", r.x + 29,
        r.y + 476, r.width - 58, 11, C_MUTED);
    if (a->xml_paths.count < XML_PATHS_MAX)
        button(a, RECT(r.x + 28, r.y + r.height - 49, 204, 32), A_XML_PATH_ADD, 0, "Ajouter un dossier",
               I_PLUS, 2, false, NULL);
    button(a, RECT(r.x + r.width - 143, r.y + r.height - 49, 114, 32), A_SETTINGS, 0, "Retour", -1, 1, false,
           NULL);
}
static void snippets_view(App *a) {
    Rectangle r = shell(a, "Macros / extraits de code",
                        "Bibliothèque commune à tous les projets · texte inséré, sans exécution", 760, 552);
    int maxscroll = a->snippets.count > 8 ? a->snippets.count - 8 : 0;
    if (a->snippet_scroll > maxscroll)
        a->snippet_scroll = maxscroll;
    if (a->snippet_scroll < 0)
        a->snippet_scroll = 0;
    char count[96];
    snprintf(count, sizeof(count), "%d / 64 extraits · ↑↓ et Entrée pour modifier", a->snippets.count);
    fit(a, count, r.x + 28, r.y + 90, r.width - 56, 11, C_MUTED);
    if (!a->snippets.count)
        fit(a, "Créez un déclencheur, par exemple xml_form, puis collez son code.", r.x + 28, r.y + 170,
            r.width - 56, 13, C_DIM);
    for (int row = 0; row < 8 && row + a->snippet_scroll < a->snippets.count; row++) {
        int index = row + a->snippet_scroll;
        Snippet *s = &a->snippets.items[index];
        float y = r.y + 125 + (float)row * 39;
        ui_box(r.x + 25, y, r.width - 50, 36, index == a->snippet_index ? C_ACCENT : C_RAISED, 4,
               index == a->snippet_index ? 14 : 255);
        icon(a, I_CODE, r.x + 36, y + 10, 16, C_ACCENT);
        ui_text_fit(&a->ui, s->trigger, r.x + 63, y + 9, 191, 12, C_WHITE, true);
        fit(a, *s->title ? s->title : "Extrait personnalisé", r.x + 270, y + 9, r.width - 388, 12, C_MUTED);
        hit(a, RECT(r.x + 25, y, r.width - 132, 36), A_SNIPPET_EDIT, index, s->title);
        button(a, RECT(r.x + r.width - 103, y + 3, 32, 30), A_SNIPPET_EDIT, index, NULL, I_CODE, 0, false,
               "Modifier le déclencheur, le titre et le code");
        button(a, RECT(r.x + r.width - 65, y + 3, 32, 30), A_SNIPPET_DELETE, index, NULL, I_TRASH, 0, false,
               "Supprimer cet extrait de la bibliothèque (avec confirmation)");
    }
    fit(a,
        *a->snippet_error ? a->snippet_error
                          : "Tapez un déclencheur dans un fichier, puis Tab ou Entrée pour insérer le code.",
        r.x + 28, r.y + 454, r.width - 56, 11, *a->snippet_error ? C_RED : C_MUTED);
    if (*a->snippet_error)
        hit(a, RECT(r.x + 28, r.y + 450, r.width - 56, 35), A_NONE, 0, a->snippet_error);
    if (a->snippets.count < SNIPPETS_MAX)
        button(a, RECT(r.x + 28, r.y + 493, 225, 34), A_SNIPPET_NEW, 0, "Nouvel extrait", I_PLUS, 2, false,
               "Créer un extrait · Ctrl+N");
    button(a, RECT(r.x + r.width - 231, r.y + 493, 203, 34), A_SETTINGS, 0, "Retour aux paramètres", -1, 1,
           false, NULL);
}
static void snippet_code(App *a, Rectangle r) {
    Buffer *b = &a->snippet_body;
    a->snippet_code_rect = r;
    buf_index(b);
    ui_box(r.x, r.y, r.width, r.height, C_BG, 5, 255);
    ui_outline(r.x, r.y, r.width, r.height, a->snippet_focus == 2 ? C_ACCENT : C_BORDER, 5, 130);
    b->scroll_y = fmaxf(0, fminf(b->scroll_y, fmaxf(0, (float)b->line_count * 25 - r.height + 20)));
    float cell = ui_width(&a->ui, "M", 17, true);
    int first = (int)(b->scroll_y / 25);
    int rows = (int)(r.height / 25) + 2;
    size_t left = 0, right = 0;
    bool selection = buf_selection(b, &left, &right);
    BeginScissorMode((int)(r.x + 4), (int)(r.y + 4), (int)(r.width - 8), (int)(r.height - 8));
    if (!b->len)
        text(a, "Collez ou saisissez votre code multiligne ici…", r.x + 11, r.y + 10, 12, C_DIM);
    for (int row = first; row < first + rows && row >= 0 && (size_t)row < b->line_count; row++) {
        size_t start = b->lines[row], end = buf_line_end(b, (size_t)row);
        float y = r.y + 8 + (float)row * 25 - b->scroll_y;
        if (selection && left <= end && right > start) {
            size_t aa = left > start ? left : start, zz = right < end ? right : end;
            float x = r.x + 10 + (float)buf_column(b, aa) * cell - b->scroll_x;
            float width = fmaxf(cell, (float)(buf_column(b, zz) - buf_column(b, aa)) * cell);
            ui_box(x, y - 1, width, 23, C_BLUE, 1, 45);
        }
        size_t col = 0;
        for (size_t p = start; p < end;) {
            size_t next = utf8_next(b->text, b->len, p);
            float x = r.x + 10 + (float)col * cell - b->scroll_x;
            if (x > r.x + r.width)
                break;
            if (b->text[p] == '\t')
                col += 4 - col % 4;
            else {
                char glyph[5];
                size_t n = next - p;
                memcpy(glyph, b->text + p, n);
                glyph[n] = 0;
                if (x + cell >= r.x)
                    mono(a, glyph, x, y, 17, C_TEXT);
                col++;
            }
            p = next;
        }
    }
    if (a->snippet_focus == 2 && ((int)(GetTime() * 2) % 2 == 0)) {
        float x = r.x + 10 + (float)buf_column(b, b->cursor) * cell - b->scroll_x;
        float y = r.y + 7 + (float)buf_row(b, b->cursor) * 25 - b->scroll_y;
        ui_box(x, y, 1, 23, C_ACCENT, 0, 255);
    }
    EndScissorMode();
    hit(a, r, A_SNIPPET_FOCUS, 2, "Code multiligne · Tab : indenter · Ctrl+Tab : changer de champ");
}
static void snippet_editor(App *a) {
    Rectangle r =
        shell(a, a->snippet_index < 0 ? "Créer un extrait de code" : "Modifier cet extrait",
              "Texte littéral · tous les projets · annulation possible avant enregistrement", 860, 568);
    label(a, "DÉCLENCHEUR", r.x + 29, r.y + 91, 9, C_ACCENT);
    label(a, "TITRE / DESCRIPTION", r.x + 285, r.y + 91, 9, C_MUTED);
    Rectangle trigger = RECT(r.x + 28, r.y + 115, 229, 36),
              title = RECT(r.x + 284, r.y + 115, r.width - 312, 36);
    app_field_draw(a, &a->snippet_trigger, trigger, a->snippet_focus == 0, "xml_form", A_NONE, false);
    app_field_draw(a, &a->snippet_title, title, a->snippet_focus == 1, "Vue formulaire Odoo", A_NONE, false);
    hit(a, trigger, A_SNIPPET_FOCUS, 0, "Nom proposé pendant la saisie : lettres, chiffres et _");
    hit(a, title, A_SNIPPET_FOCUS, 1, "Description facultative affichée avec la proposition");
    label(a, "CODE À INSÉRER", r.x + 29, r.y + 169, 9, C_ACCENT);
    snippet_code(a, RECT(r.x + 28, r.y + 193, r.width - 56, 269));
    fit(a,
        *a->snippet_error
            ? a->snippet_error
            : "Entrée : nouvelle ligne · Tab : indenter · Ctrl+Tab : changer de champ · Ctrl+S : enregistrer",
        r.x + 28, r.y + 478, r.width - 56, 11, *a->snippet_error ? C_RED : C_MUTED);
    if (*a->snippet_error)
        hit(a, RECT(r.x + 28, r.y + 473, r.width - 56, 34), A_NONE, 0, a->snippet_error);
    button(a, RECT(r.x + 28, r.y + 515, 128, 34), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
    button(a, RECT(r.x + r.width - 211, r.y + 515, 183, 34), A_SNIPPET_SAVE, 0, "Enregistrer", I_SAVE, 2,
           false, "Enregistrer immédiatement dans la bibliothèque commune · Ctrl+S");
}
static void snippet_confirm(App *a) {
    bool deleting = a->modal == M_SNIPPET_DELETE;
    Rectangle r = shell(a, deleting ? "Supprimer cet extrait ?" : "Conserver vos modifications ?",
                        deleting ? "Seule cette macro sera retirée. Aucun fichier du projet ne sera supprimé."
                                 : "Le code de cet extrait contient des modifications non enregistrées.",
                        760, 260);
    const char *name = deleting && a->snippet_index >= 0 && a->snippet_index < a->snippets.count
                           ? a->snippets.items[a->snippet_index].trigger
                           : a->snippet_trigger.text;
    ui_text_fit(&a->ui, name && *name ? name : "Nouvel extrait", r.x + 28, r.y + 110, r.width - 56, 17,
                C_WHITE, true);
    fit(a, a->snippet_error, r.x + 28, r.y + 150, r.width - 56, 11, C_RED);
    button(a, RECT(r.x + 28, r.y + 200, deleting ? 140 : 220, 34),
           deleting ? A_MODAL_CLOSE : A_SNIPPET_CONTINUE, 0, deleting ? "Annuler" : "Continuer l’édition", -1,
           1, false, NULL);
    if (deleting)
        button(a, RECT(r.x + r.width - 211, r.y + 200, 183, 34), A_SNIPPET_CONFIRM_DELETE, 0, "Supprimer",
               I_TRASH, 3, false, NULL);
    else {
        button(a, RECT(r.x + r.width - 383, r.y + 200, 150, 34), A_SNIPPET_DISCARD, 0, "Abandonner", -1, 1,
               false, NULL);
        button(a, RECT(r.x + r.width - 211, r.y + 200, 183, 34), A_SNIPPET_SAVE, 0, "Enregistrer", I_SAVE, 2,
               false, NULL);
    }
}
static void help(App *a) {
    const char *keys[] = {"F11 / Alt Entrée",
                          "F7 / Ctrl \\",
                          "Ctrl 1 / Ctrl 2",
                          "Alt clic",
                          "Ctrl Alt Haut / Bas",
                          "Ctrl D / Ctrl Maj L",
                          "Échap",
                          "Ctrl N / Ctrl O",
                          "Ctrl S / Ctrl Maj S",
                          "Ctrl Z / Ctrl Y",
                          "Ctrl F / G / Maj F",
                          "Ctrl P / Ctrl Maj P",
                          "Ctrl Espace",
                          "Tab / Maj Tab",
                          "Ctrl / / Ctrl Maj D",
                          "F5 / Maj F5",
                          "Ctrl J / Ctrl B",
                          "F8",
                          "F9",
                          "F6"};
    const char *desc[] = {"Plein écran / fenêtre",
                          "Deux fichiers côte à côte",
                          "Activer le volet gauche / droit",
                          "Ajouter / retirer un curseur",
                          "Ajouter un curseur haut / bas (répétable)",
                          "Occurrence suivante / toutes les occurrences",
                          "Revenir à un curseur / fermer la recherche",
                          "Nouveau fichier / ouvrir",
                          "Enregistrer / enregistrer sous",
                          "Annuler / rétablir",
                          "Fichier / ligne / dossier",
                          "Fichiers / commandes",
                          "Complétion (un seul curseur)",
                          "Indenter / désindenter",
                          "Commenter / dupliquer",
                          "Exécuter / arrêter",
                          "Console / explorateur",
                          "Menu du haut : afficher / replier",
                          "Tout replier / restaurer les panneaux",
                          "Activer / désactiver les effets"};
    Rectangle r = shell(a, "Plusieurs idées, un même espace.",
                        "Dossier : Ctrl+Maj+F · F3/Maj+F3 · agrandir la recherche : F11", 750, 550);
    for (int i = 0; i < 20; i++) {
        float y = r.y + 99 + (float)i * 20;
        if (i % 2 == 0)
            ui_box(r.x + 19, y - 4, r.width - 38, 20, C_RAISED, 3, 150);
        mono(a, keys[i], r.x + 31, y + 1, 10, C_ACCENT);
        text(a, desc[i], r.x + 268, y + 1, 11, C_TEXT);
    }
    text(a, "Python Studio C " PRISME_VERSION " · C11 + raylib 5.5 · OpenGL 2.1 par défaut", r.x + 30,
         r.y + r.height - 28, 10, C_DIM);
}
static void xml_dialog(App *a) {
    XmlInlineState *x = &a->xml;
    Rectangle r = shell(a, "Identifiants XML",
                        x->refs.raw ? x->refs.path : "Fichier lu uniquement sur demande · arch vide",
                        fminf(790, a->w - 40), fminf(560, a->h - 40));
    app_field_draw(a, &a->field, RECT(r.x + 26, r.y + 88, r.width - 52, 36), true,
                   "Filtrer les identifiants, balises et modèles…", A_NONE, false);
    int rows = (int)((r.height - 225) / 46);
    if (rows > 7)
        rows = 7;
    if (rows < 1)
        rows = 1;
    int first = x->index >= rows ? x->index - rows + 1 : 0;
    for (int row = 0; row < rows && first + row < x->count; row++) {
        int index = first + row;
        XmlIdentifier *item = &x->refs.items[x->filtered[index]];
        float y = r.y + 140 + (float)row * 46;
        bool active = index == x->index;
        if (active)
            ui_box(r.x + 18, y, r.width - 36, 43, C_ACCENT, 4, 13);
        char label[1024];
        strcpy(label, item->id);
        for (size_t i = 0; label[i]; i++)
            if (label[i] == '\r' || label[i] == '\n' || label[i] == '\t')
                label[i] = ' ';
        fit(a, label, r.x + 33, y + 5, r.width - 82, 13, active ? C_ACCENT : C_TEXT);
        char detail[1400];
        snprintf(detail, sizeof(detail), "<%s>%s%s · ligne %ld", item->tag, *item->model ? " · " : "",
                 item->model, item->line);
        for (size_t i = 0; detail[i]; i++)
            if (detail[i] == '\r' || detail[i] == '\n' || detail[i] == '\t')
                detail[i] = ' ';
        fit(a, detail, r.x + 33, y + 25, r.width - 82, 11, C_MUTED);
        hit(a, RECT(r.x + 18, y, r.width - 36, 43), A_XML_PICK, index,
            "Remplacer le chemin par ce bloc XML (arch vide)");
    }
    if (!x->refs.raw)
        fit(a, *x->error ? x->error : "Lecture XML en cours… Échap : annuler, sans modifier le chemin.",
            r.x + 29, r.y + 150, r.width - 58, 12, *x->error ? C_RED : C_MUTED);
    else if (!x->count)
        fit(a,
            x->refs.count ? "Aucun identifiant correspondant."
                          : "Aucun attribut id non vide dans ce fichier XML.",
            r.x + 29, r.y + 150, r.width - 58, 12, C_MUTED);
    char count[160];
    snprintf(count, sizeof(count), "%d / %d identifiants · Entrée : insérer · Échap : annuler", x->count,
             x->refs.count);
    fit(a, count, r.x + 28, r.y + r.height - 65, r.width - 56, 11, C_MUTED);
    button(a, RECT(r.x + r.width - 279, r.y + r.height - 48, 115, 32), A_MODAL_CLOSE, 0, "Annuler", -1, 1,
           false, NULL);
    if (x->count)
        button(a, RECT(r.x + r.width - 150, r.y + r.height - 48, 122, 32), A_XML_PICK, x->index, "Insérer",
               I_CODE, 2, false, NULL);
}
static void context_menu(App *a) {
    PopupLayout r = layout_context(a->w, a->h, a->layout.status_y, (int)a->context_position.x,
                                   (int)a->context_position.y);
    ui_box(r.x + 4, r.y + 6, r.width, r.height, C_BLACK, 8, 100);
    ui_box(r.x, r.y, r.width, r.height, C_RAISED, 8, 255);
    ui_outline(r.x, r.y, r.width, r.height, C_DIM, 8, 160);
    const char *name = *a->context_target ? fs_basename(a->context_target) : "Dossier du projet";
    fit(a, *name ? name : "Dossier du projet", r.x + 14, r.y + 13, r.width - 28, 12, C_WHITE);
    ui_line(r.x + 10, r.y + 41, r.x + r.width - 10, r.y + 41, C_BORDER, 1, 255);
    hit(a, RECT(r.x, r.y, r.width, r.height), A_NONE, 0, NULL);
    const char *labels[] = {"Créer un fichier…", "Créer un dossier…", "Copier", "Coller ici", "Supprimer…"};
    const Icon icons[] = {I_FILE, I_FOLDER, I_FILES, I_SAVE, I_TRASH};
    for (int i = 0; i < 5; i++) {
        float y = r.y + 48 + (float)i * LAYOUT_CONTEXT_ROW_H;
        bool enabled = app_context_enabled(a, i);
        Rectangle row = RECT(r.x + 6, y, r.width - 12, 34);
        bool hover = ui_inside(row, a->ui.mouse);
        if (enabled && (hover || a->context_index == i))
            ui_box(row.x, row.y, row.width, row.height, C_ACCENT, 4, 14);
        Tone tone = !enabled ? C_DIM : i == 4 ? C_RED : C_TEXT;
        icon(a, icons[i], r.x + 16, y + 8, 17, tone);
        fit(a, labels[i], r.x + 45, y + 8, r.width - 58, 12, tone);
        if (enabled)
            hit(a, row, app_context_action(i), 0, NULL);
    }
}
static void filesystem_dialog(App *a) {
    if (a->modal == M_FS_CREATE) {
        Rectangle r = shell(a, a->create_directory ? "Créer un dossier" : "Créer un fichier",
                            "Choisissez un nom. Aucun élément existant ne sera écrasé.", 640, 302);
        app_field_draw(a, &a->field, RECT(r.x + 28, r.y + 98, r.width - 56, 38), true, "Nom…", A_NONE, false);
        fit(a, a->context_parent, r.x + 28, r.y + 153, r.width - 56, 12, C_MUTED);
        fit(a, a->fs_error, r.x + 28, r.y + 187, r.width - 56, 12, C_RED);
        button(a, RECT(r.x + 28, r.y + 240, 110, 35), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
        button(a, RECT(r.x + r.width - 158, r.y + 240, 130, 35), A_FS_CREATE, 0, "Créer", I_PLUS, 2, false,
               NULL);
    } else if (a->modal == M_FS_DELETE) {
        Rectangle r = shell(a, "Supprimer cet élément ?",
                            "Suppression définitive : cette action ne peut pas être annulée.", 640, 302);
        fit(a, a->context_target, r.x + 28, r.y + 105, r.width - 56, 14, C_WHITE);
        text(a, "Un dossier sera supprimé avec tout son contenu.", r.x + 28, r.y + 145, 12, C_MUTED);
        fit(a, a->fs_error, r.x + 28, r.y + 187, r.width - 56, 12, C_RED);
        button(a, RECT(r.x + 28, r.y + 240, 110, 35), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
        button(a, RECT(r.x + r.width - 178, r.y + 240, 150, 35), A_FS_CONFIRM_DELETE, 0, "Supprimer", I_TRASH,
               3, false, NULL);
    } else if (a->file_job) {
        FileJob *job = a->file_job;
        Rectangle r = shell(a, job->deleting ? "Suppression en cours…" : "Copie en cours…",
                            "L’interface reste réactive pendant cette opération.", 640, 302);
        char progress[160];
        snprintf(progress, sizeof(progress), "%llu éléments · %.1f Mo",
                 (unsigned long long)atomic_load(&job->entries),
                 (double)atomic_load(&job->bytes) / (1024 * 1024));
        text(a, progress, r.x + 28, r.y + 108, 17, C_ACCENT);
        fit(a, job->deleting ? job->target : job->parent, r.x + 28, r.y + 151, r.width - 56, 12, C_MUTED);
        if (!job->deleting) {
            fit(a,
                atomic_load(&job->cancel) ? "Annulation… nettoyage des copies partielles."
                                          : "Les fichiers existants ne seront pas remplacés.",
                r.x + 28, r.y + 190, r.width - 56, 12, C_DIM);
            button(a, RECT(r.x + r.width - 228, r.y + 240, 200, 35), A_FS_CANCEL, 0, "Annuler la copie", -1,
                   1, false, NULL);
        } else
            text(a, "Veuillez patienter jusqu’à la fin de la suppression.", r.x + 28, r.y + 200, 12, C_DIM);
    }
}
static void modal(App *a) {
    a->ui.layer = 1;
    if (a->modal == M_SSH || a->modal == M_SSH_PROFILES || a->modal == M_SSH_AUTH)
        app_ssh_draw(a);
    else if (a->modal == M_FOLDER_SEARCH)
        app_folder_search_draw(a);
    else if (a->modal == M_XML_IDS)
        xml_dialog(a);
    else if (a->modal == M_CONTEXT)
        context_menu(a);
    else if (a->modal == M_FS_CREATE || a->modal == M_FS_DELETE || a->modal == M_FS_BUSY)
        filesystem_dialog(a);
    else if (a->modal == M_PALETTE)
        palette(a);
    else if (a->modal == M_BROWSER)
        browser(a);
    else if (a->modal == M_SETTINGS)
        settings(a);
    else if (a->modal == M_LINT_RULES)
        lint_rules_settings(a);
    else if (a->modal == M_ODOO)
        odoo_settings(a);
    else if (a->modal == M_IMPORTS)
        imports_settings(a);
    else if (a->modal == M_XML_PATHS)
        xml_paths_settings(a);
    else if (a->modal == M_SNIPPETS)
        snippets_view(a);
    else if (a->modal == M_SNIPPET_EDIT)
        snippet_editor(a);
    else if (a->modal == M_SNIPPET_DELETE || a->modal == M_SNIPPET_DIRTY)
        snippet_confirm(a);
    else if (a->modal == M_HELP)
        help(a);
    else if (a->modal == M_GOTO) {
        char subtitle[96];
        snprintf(subtitle, sizeof(subtitle), "Ce fichier contient %zu lignes.", app_buffer(a)->line_count);
        Rectangle r = shell(a, "Aller à la ligne", subtitle, 480, 214);
        app_field_draw(a, &a->field, RECT(r.x + 28, r.y + 91, r.width - 56, 37), true, "Numéro de ligne",
                       A_NONE, false);
        button(a, RECT(r.x + r.width - 155, r.y + 153, 126, 33), A_GOTO_SUBMIT, 0, "Aller à la ligne", -1, 2,
               false, NULL);
    } else if (a->modal == M_UNSAVED) {
        Rectangle r =
            shell(a, "Gardons vos idées.", "Ce fichier contient des modifications non enregistrées.", 600,
                  a->pending == 2 && a->session.enabled ? 300 : 245);
        icon(a, I_FILE, r.x + 31, r.y + 104, 20, C_ACCENT);
        ui_text_fit(&a->ui, app_buffer(a)->name, r.x + 65, r.y + 108, r.width - 94, 14, C_WHITE, true);
        button(a, RECT(r.x + 25, r.y + 183, 100, 34), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false, NULL);
        button(a, RECT(r.x + 149, r.y + 183, 205, 34), A_UNSAVED_DISCARD, 0, "Ne pas enregistrer", -1, 0,
               false, NULL);
        button(a, RECT(r.x + r.width - 201, r.y + 183, 175, 34), A_UNSAVED_SAVE, 0, "Enregistrer", I_SAVE, 2,
               false, NULL);
        if (a->pending == 2 && a->session.enabled)
            button(a, RECT(r.x + 25, r.y + 239, r.width - 50, 34), A_SESSION_KEEP_QUIT, 0,
                   "Conserver les brouillons et quitter", -1, 1, false, NULL);
    } else if (a->modal == M_OVERWRITE) {
        Rectangle r = shell(a, "Remplacer ce fichier ?", "Un fichier portant ce nom existe déjà.", 560, 235);
        ui_text_fit(&a->ui, fs_basename(a->save_target), r.x + 29, r.y + 109, r.width - 58, 14, C_TEXT, true);
        button(a, RECT(r.x + r.width - 258, r.y + 176, 100, 34), A_MODAL_CLOSE, 0, "Annuler", -1, 1, false,
               NULL);
        button(a, RECT(r.x + r.width - 141, r.y + 176, 113, 34), A_OVERWRITE, 0, "Remplacer", -1, 3, false,
               NULL);
    }
}
void app_draw(App *a, float dt) {
    app_layout(a);
    BeginDrawing();
    ClearBackground(ui_colors[C_BG]);
    ui_begin(&a->ui);
    if (a->topbar)
        topbar(a);
    if (a->sidebar && a->ex > 0)
        sidebar(a);
    tabs(a);
    int focused = a->active_pane;
    a->panes[focused].tab = a->active;
    for (int pane = 0; pane < (a->split ? 2 : 1); pane++) {
        app_view_pane(a, pane);
        editor(a, dt);
        if (a->split)
            pane_header(a, pane);
    }
    app_view_pane(a, focused);
    if (a->split) {
        BOX(a->divider_x - 2, a->layout.crumb_y, 4, a->code_bottom - a->layout.crumb_y, C_BORDER);
        ui_box(a->divider_x - 1, a->layout.crumb_y + 9, 2, 23, C_ACCENT, 1, 90);
    }
    if (a->terminal && a->layout.console_h > 0)
        console(a);
    status(a);
    if ((a->completion_count || a->completion_pending) && !a->modal && !a->focus)
        completions(a);
    if (*a->toast && GetTime() - a->toast_time < 3.5) {
        float w = fminf((float)a->w - 60, ui_width(&a->ui, a->toast, 12, false) + 60),
              x = (float)a->w - w - 22, y = (float)a->layout.status_y - 50;
        ui_box(x + 3, y + 5, w, 37, C_BLACK, 7, 60);
        ui_box(x, y, w, 37, C_RAISED, 7, 255);
        ui_outline(x, y, w, 37, a->toast_error ? C_RED : C_ACCENT, 7, 50);
        icon(a, a->toast_error ? I_WARNING : I_CHECK, x + 13, y + 10, 16, a->toast_error ? C_RED : C_ACCENT);
        text(a, a->toast, x + 40, y + 13, 12, C_TEXT);
    }
    if (a->modal)
        modal(a);
    ui_tooltip(&a->ui, a->modal != M_NONE);
    EndDrawing();
}
