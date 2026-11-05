#include "app_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define RECT(x, y, w, h) ((Rectangle){(float)(x), (float)(y), (float)(w), (float)(h)})

/* Only materialize the visible UTF-8 cells. Tabs follow the search core's
   scalar-cell convention, including a viewport starting inside a tab. */
size_t app_folder_search_line(Buffer *b, unsigned row, unsigned left, unsigned columns, char *out,
                              size_t cap) {
    if (!out || !cap)
        return 0;
    out[0] = 0;
    if (!b || !b->text || !columns)
        return 0;
    buf_index(b);
    if (row >= b->line_count)
        return 0;
    size_t p = b->lines[row], end = buf_line_end(b, row), used = 0;
    unsigned column = 0;
    while (p < end && column < left + columns) {
        size_t next = utf8_next(b->text, end, p);
        if (b->text[p] == '\t') {
            unsigned spaces = 4 - column % 4;
            for (unsigned i = 0; i < spaces; i++, column++)
                if (column >= left && column < left + columns && used + 1 < cap)
                    out[used++] = ' ';
        } else {
            if (column >= left) {
                size_t bytes = next - p;
                if (bytes >= cap - used)
                    break;
                memcpy(out + used, b->text + p, bytes);
                used += bytes;
            }
            column++;
        }
        p = next;
    }
    out[used] = 0;
    return used;
}
static void text(App *a, const char *value, float x, float y, float size, Tone tone) {
    ui_text(&a->ui, value, x, y, size, tone, false);
}
static void fit(App *a, const char *value, float x, float y, float width, float size, Tone tone) {
    ui_text_fit(&a->ui, value, x, y, width, size, tone, false);
}
static void button(App *a, Rectangle r, Action action, int arg, const char *label, int icon, int style,
                   const char *tip) {
    ui_button(&a->ui, r, action, arg, label, icon, style, false, tip);
}
static unsigned first_line_hit(const SearchFile *f, size_t start) {
    unsigned lo = 0, hi = f->count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (f->hits[mid].end <= start && f->hits[mid].start < start)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}
void app_folder_search_draw(App *a) {
    FolderSearchState *s = &a->folder_search;
    if (!s->initialized)
        return;
    Rectangle shell;
    if (s->maximized) {
        shell = RECT(0, 0, (float)a->w, (float)a->h);
        ui_box(0, 0, shell.width, shell.height, C_PANEL, 0, 255);
        text(a, "Rechercher dans un dossier", 28, 26, 19, C_WHITE);
        fit(a, "Instantanés en lecture seule · F3 global · résultats au fil du scan", 29, 57,
            shell.width - 58, 11, C_MUTED);
        button(a, RECT(shell.width - 52, 20, 30, 30), A_MODAL_CLOSE, 0, NULL, I_CLOSE, 0, "Fermer · Échap");
    } else
        shell = app_ssh_shell(a, "Rechercher dans un dossier",
                              "Instantanés en lecture seule · F3 global · résultats au fil du scan",
                              fminf(1240, (float)a->w - 32), (float)a->h - 48);
    if (s->shell_rect.x != shell.x || s->shell_rect.y != shell.y || s->shell_rect.width != shell.width ||
        s->shell_rect.height != shell.height)
        s->scroll_drag = 0;
    s->shell_rect = shell;
    float x = shell.x, y = shell.y, w = shell.width, h = shell.height;
    button(a, RECT(x + w - 252, y + 20, 180, 30), A_FOLDER_SEARCH_MAXIMIZE, 0,
           s->maximized ? "Restaurer · F11" : "Agrandir · F11", I_EXPAND, 1,
           "Agrandir uniquement la recherche ; la fenêtre de l’éditeur ne change pas");
    text(a, "Dossier", x + 25, y + 96, 12, C_MUTED);
    app_field_draw(a, &s->folder, RECT(x + 98, y + 84, w - 180, 36), s->focus == 0, "Dossier de recherche…",
                   A_FOLDER_SEARCH_FOCUS, false);
    ui_hit(&a->ui, RECT(x + 98, y + 84, w - 180, 36), A_FOLDER_SEARCH_FOCUS, 0, NULL);
    button(a, RECT(x + w - 71, y + 84, 45, 36), A_FOLDER_SEARCH_PICK_ROOT, 0, NULL, I_FOLDER, 1,
           "Choisir et mémoriser un dossier");
    text(a, "Texte", x + 25, y + 141, 12, C_MUTED);
    app_field_draw(a, &s->query, RECT(x + 98, y + 129, w - 278, 36), s->focus == 1, "Chaîne à rechercher…",
                   A_NONE, false);
    ui_hit(&a->ui, RECT(x + 98, y + 129, w - 278, 36), A_FOLDER_SEARCH_FOCUS, 1, NULL);
    bool busy = s->job && !atomic_load(&s->job->done);
    button(a, RECT(x + w - 168, y + 129, 142, 36), busy ? A_FOLDER_SEARCH_STOP : A_FOLDER_SEARCH_START, 0,
           busy ? "Arrêter" : "Rechercher", busy ? I_STOP : I_SEARCH, busy ? 1 : 2,
           "Entrée / Ctrl+Entrée : rechercher ; aucune recherche sur la frappe");
    bool checked[] = {s->options.case_sensitive, s->options.whole_word, s->options.regex,
                      s->options.recursive};
    const char *labels[] = {"Casse", "Mots entiers", "Regex", "Sous-dossiers"};
    float slot = (w - 178) / 4;
    for (int i = 0; i < 4; i++) {
        Rectangle r = RECT(x + 25 + (float)i * slot, y + 174, slot - 8, 30);
        char label[80];
        snprintf(label, sizeof(label), "[%c] %s", checked[i] ? 'x' : ' ', labels[i]);
        button(a, r, A_FOLDER_SEARCH_OPTION, i, label, -1, checked[i] ? 2 : 1,
               checked[i] ? "Option active" : "Option inactive");
        if (s->focus == 8 + i)
            ui_outline(r.x, r.y, r.width, r.height, C_ACCENT, 4, 255);
    }
    button(a, RECT(x + w - 144, y + 174, 118, 30), A_FOLDER_SEARCH_FILTERS, 0,
           s->filters_open ? "Filtres −" : "Filtres +", -1, 1,
           "Inclure/exclure fichiers et dossiers ; motifs * ? ** séparés par ;");
    float extra = s->filters_open ? 122 : 0;
    if (s->filters_open) {
        const char *captions[] = {"Fichiers inclus · *.py;*.xml", "Fichiers exclus · *.min.js",
                                  "Dossiers inclus · src;addons", "Dossiers exclus · tests;build*"};
        float colw = (w - 62) / 2;
        for (int i = 0; i < 4; i++) {
            float xx = x + 25 + (float)(i % 2) * (colw + 12), yy = y + 212 + (float)(i / 2) * 58;
            text(a, captions[i], xx, yy, 11, C_MUTED);
            Rectangle r = RECT(xx, yy + 17, colw, 32);
            app_field_draw(a, &s->filters[i], r, s->focus == 4 + i,
                           i % 2 ? "Aucun filtre supplémentaire" : "Tous", A_NONE, false);
            ui_hit(&a->ui, r, A_FOLDER_SEARCH_FOCUS, 4 + i,
                   "Globs relatifs : * ne traverse pas /, ** le traverse ; séparateur ;. Dossiers VCS/cache "
                   "toujours ignorés.");
        }
    }
    SearchReport *report = s->report;
    char status[512];
    if (busy)
        snprintf(status, sizeof(status), "Recherche… %u fichiers examinés · %u trouvés · %u occurrences",
                 atomic_load(&s->job->progress_examined), atomic_load(&s->job->progress_found),
                 atomic_load(&s->job->progress_hits));
    else if (report)
        snprintf(status, sizeof(status), "%u fichiers · %u occurrences · %u examinés · %u écartés%s",
                 report->count, report->hit_count, report->examined, report->skipped + report->ignored,
                 report->cancelled   ? " · recherche arrêtée (partielle)"
                 : report->limited   ? " · limite atteinte (partielle)"
                 : !report->complete ? " · résultats partiels"
                                     : "");
    else
        snprintf(status, sizeof(status),
                 "Dossier mémorisé par projet ; ni le texte recherché ni les résultats ne sont enregistrés.");
    fit(a, *s->error ? s->error : status, x + 26, y + 218 + extra, w - 54, 12, *s->error ? C_RED : C_MUTED);
    float panel_y = y + 246 + extra, panel_h = fmaxf(80, h - 340 - extra),
          left_w = fminf(340, fmaxf(220, (w - 60) * .30f));
    Rectangle left = RECT(x + 24, panel_y, left_w, panel_h),
              right = RECT(x + left_w + 36, panel_y, w - left_w - 60, panel_h);
    if (s->content_rect.x != right.x || s->content_rect.y != right.y ||
        s->content_rect.width != right.width || s->content_rect.height != right.height)
        s->scroll_drag = 0;
    s->list_rect = left;
    s->content_rect = right;
    s->list_rows = (unsigned)fmaxf(1, floorf((panel_h - 38) / 43));
    Rectangle panes[] = {left, right};
    for (unsigned i = 0; i < 2; i++) {
        ui_box(panes[i].x, panes[i].y, panes[i].width, panes[i].height, C_BG, 5, 255);
        ui_outline(panes[i].x, panes[i].y, panes[i].width, panes[i].height, C_BORDER, 5, 255);
    }
    text(a, "FICHIERS", left.x + 13, left.y + 12, 11, C_MUTED);
    ui_line(left.x + 1, left.y + 34, left.x + left.width - 1, left.y + 34, C_BORDER, 1, 255);
    unsigned count = report ? report->count : 0, last = count > s->list_rows ? count - s->list_rows : 0;
    if (s->scroll > last)
        s->scroll = last;
    for (unsigned row = 0; row < s->list_rows && s->scroll + row < count; row++) {
        unsigned index = s->scroll + row;
        SearchFile *f = &report->files[index];
        float yy = left.y + 38 + (float)row * 43;
        bool selected = (int)index == s->file;
        if (selected)
            ui_box(left.x + 4, yy, left.width - 8, 40, C_ACCENT, 3, 14);
        ui_icon(&a->ui, I_FILE, left.x + 12, yy + 8, 14, selected ? C_ACCENT : C_MUTED, 255);
        fit(a, fs_basename(f->path), left.x + 34, yy + 4, left.width - 91, 12, selected ? C_WHITE : C_TEXT);
        char number[32];
        snprintf(number, sizeof(number), "%u", f->count);
        ui_text(&a->ui, number, left.x + left.width - 47, yy + 5, 12, C_ACCENT, true);
        fit(a, f->relative, left.x + 14, yy + 24, left.width - 46, 10, C_DIM);
        ui_hit(&a->ui, RECT(left.x + 4, yy, left.width - 8, 40), A_FOLDER_SEARCH_FILE, (int)index, f->path);
    }
    if (count > s->list_rows) {
        float span = left.height - 39, thumb = fmaxf(14, span * (float)s->list_rows / (float)count);
        float py = left.y + 37 + (span - thumb) * (float)s->scroll / (float)(count - s->list_rows);
        ui_box(left.x + left.width - 5, py, 3, thumb, C_DIM, 1, 120);
    }
    SearchFile *active = report && s->file >= 0 && (unsigned)s->file < count ? &report->files[s->file] : NULL;
    fit(a, active ? active->relative : "Contenu du fichier", right.x + 13, right.y + 11, right.width - 26, 12,
        C_WHITE);
    ui_line(right.x + 1, right.y + 34, right.x + right.width - 1, right.y + 34, C_BORDER, 1, 255);
    float size = fminf(22, fmaxf(17, (float)a->font_size)), line_h = size * 1.38f;
    float cw = fmaxf(1, ui_width(&a->ui, "M", size, true)), code_x = right.x + 66, code_y = right.y + 42;
    s->rows = (unsigned)fmaxf(1, floorf((right.height - 62) / line_h));
    s->columns = (unsigned)fmaxf(1, floorf((right.width - 86) / cw));
    if (active && s->view.text && !s->extent_columns)
        s->extent_columns = folder_search_columns(s->view.text, s->view.len) + 1;
    unsigned max_x = s->extent_columns > s->columns ? s->extent_columns - s->columns : 0;
    unsigned max_y = s->view.line_count > s->rows ? (unsigned)s->view.line_count - s->rows : 0;
    if (s->x > max_x)
        s->x = max_x;
    if (s->y > max_y)
        s->y = max_y;
    ui_hit(&a->ui, RECT(right.x + 1, right.y + 36, right.width - 20, right.height - 56),
           A_FOLDER_SEARCH_FOCUS, 3,
           "Lecture seule · molette : défiler · Maj+molette : horizontal · F3 : occurrence suivante");
    if (active && s->view.text) {
        BeginScissorMode((int)(right.x + 2), (int)(right.y + 36), (int)fmaxf(1, right.width - 22),
                         (int)fmaxf(1, right.height - 56));
        unsigned hit_index = first_line_hit(active, s->view.lines[s->y < s->view.line_count ? s->y : 0]);
        for (unsigned row = 0; row <= s->rows && s->y + row < s->view.line_count; row++) {
            unsigned number = s->y + row;
            float yy = code_y + (float)row * line_h;
            char caption[32];
            snprintf(caption, sizeof(caption), "%u", number + 1);
            ui_text(&a->ui, caption, right.x + 8, yy + 1, 12, C_DIM, true);
            size_t line_start = s->view.lines[number], line_end = buf_line_end(&s->view, number);
            while (hit_index < active->count && active->hits[hit_index].end <= line_start &&
                   active->hits[hit_index].start < line_start)
                hit_index++;
            size_t scan = line_start;
            unsigned column = 0;
            for (unsigned i = hit_index; i < active->count && active->hits[i].start <= line_end; i++) {
                SearchHit *hit = &active->hits[i];
                size_t start = hit->start > line_start ? hit->start : line_start,
                       end_byte = hit->end < line_end ? hit->end : line_end;
                while (scan < start) {
                    column += s->view.text[scan] == '\t' ? 4 - column % 4 : 1;
                    scan = utf8_next(s->view.text, s->view.len, scan);
                }
                unsigned from = column;
                while (scan < end_byte) {
                    column += s->view.text[scan] == '\t' ? 4 - column % 4 : 1;
                    scan = utf8_next(s->view.text, s->view.len, scan);
                }
                unsigned width = column - from;
                if (!width)
                    width = 1;
                int begin = (int)from - (int)s->x, end = (int)(from + width) - (int)s->x;
                if (begin < 0)
                    begin = 0;
                if (end > (int)s->columns)
                    end = (int)s->columns;
                if (end <= begin)
                    continue;
                bool selected = (int)i == s->hit;
                ui_box(code_x + (float)begin * cw, yy - 1, (float)(end - begin) * cw, line_h - 1,
                       selected ? C_ACCENT : C_NUMBER, 2, selected ? 70 : 42);
                if (selected)
                    ui_box(code_x + (float)begin * cw, yy + line_h - 3, (float)(end - begin) * cw, 2,
                           C_ACCENT, 0, 255);
            }
            char visible[8192];
            app_folder_search_line(&s->view, number, s->x, s->columns, visible, sizeof(visible));
            ui_text(&a->ui, visible, code_x, yy, size, C_TEXT, true);
        }
        EndScissorMode();
        s->htrack = RECT(code_x, right.y + right.height - 16, fmaxf(1, right.width - 86), 12);
        s->vtrack = RECT(right.x + right.width - 16, right.y + 38, 12, fmaxf(1, right.height - 58));
        s->horizontal = layout_scroll_axis(s->htrack.x, s->htrack.width, s->columns, s->extent_columns, s->x);
        s->vertical =
            layout_scroll_axis(s->vtrack.y, s->vtrack.height, s->rows, (unsigned)s->view.line_count, s->y);
        s->hthumb = RECT(s->horizontal.thumb_start, s->htrack.y, s->horizontal.thumb_span, s->htrack.height);
        s->vthumb = RECT(s->vtrack.x, s->vertical.thumb_start, s->vtrack.width, s->vertical.thumb_span);
        Rectangle tracks[] = {s->htrack, s->vtrack}, thumbs[] = {s->hthumb, s->vthumb};
        ScrollAxis axes[] = {s->horizontal, s->vertical};
        for (int i = 0; i < 2; i++) {
            Rectangle track = tracks[i], thumb = thumbs[i];
            ui_box(track.x, track.y, track.width, track.height, C_RAISED, 4, 255);
            bool held = s->scroll_drag == i + 1 || ui_inside(thumb, a->ui.mouse);
            ui_box(thumb.x, thumb.y, thumb.width, thumb.height,
                   held && axes[i].limit ? C_ACCENT
                   : axes[i].limit       ? C_MUTED
                                         : C_BORDER,
                   4, 255);
            ui_hit(&a->ui, track, A_FOLDER_SEARCH_FOCUS, 3,
                   i == 0 ? "Défilement horizontal · cliquer ou faire glisser ; lecture seule"
                          : "Défilement vertical · cliquer ou faire glisser ; lecture seule");
        }
    } else {
        s->scroll_drag = 0;
        s->htrack = s->hthumb = s->vtrack = s->vthumb = (Rectangle){0};
        fit(a, report ? "Choisissez un fichier trouvé." : "Lancez une recherche pour afficher les fichiers.",
            right.x + 14, right.y + 62, right.width - 30, 12, C_DIM);
    }
    char message[1024];
    snprintf(message, sizeof(message), "%s", s->note);
    if (active && s->hit >= 0 && (unsigned)s->hit < active->count) {
        unsigned ordinal = (unsigned)s->hit + 1;
        for (int i = 0; i < s->file; i++)
            ordinal += report->files[i].count;
        const char *phase = busy ? "Recherche en cours · "
                            : (!report->complete || report->cancelled || report->limited)
                                ? "Résultats partiels · "
                                : "";
        snprintf(message, sizeof(message),
                 "%sOccurrence %u/%u · fichier %d/%u · ligne %u · instantané du disque", phase, ordinal,
                 report->hit_count, s->file + 1, count, active->hits[s->hit].line + 1);
    }
    SearchOptions current = s->options;
    char *filters[] = {current.include_files, current.exclude_files, current.include_dirs,
                       current.exclude_dirs};
    for (unsigned i = 0; i < 4; i++)
        snprintf(filters[i], SEARCH_FILTER_MAX + 1, "%s", s->filters[i].text ? s->filters[i].text : "");
    if (report && (strcmp(s->folder.text, report->root) || strcmp(s->query.text, report->query) ||
                   !folder_search_options_equal(&current, &report->options)))
        snprintf(message, sizeof(message),
                 "Paramètres modifiés — résultats de l’ancienne recherche ; relancez pour actualiser.");
    fit(a, message, x + 26, y + h - 77, w - 52, 11, C_MUTED);
    float by = y + h - 51;
    button(a, RECT(x + 25, by, 145, 33), A_FOLDER_SEARCH_PREV, 0, "Maj+F3", I_LEFT, 1,
           "Occurrence précédente dans tous les fichiers");
    button(a, RECT(x + 180, by, 145, 33), A_FOLDER_SEARCH_NEXT, 0, "Suivant · F3", I_RIGHT, 1,
           "Occurrence suivante, y compris dans le fichier suivant");
    if (active)
        button(a, RECT(x + 337, by, 196, 33), A_FOLDER_SEARCH_OPEN, 0, "Ouvrir dans l’éditeur", -1, 1,
               "Ouvre le fichier actuel ; ne remplace pas les modifications d’un onglet déjà ouvert");
    button(a, RECT(x + w - 122, by, 96, 33), A_MODAL_CLOSE, 0, "Fermer", -1, 1, NULL);
}
