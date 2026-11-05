#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

const Color ui_colors[C_COUNT] = {[C_BG] = {16, 18, 24, 255},
                                  [C_PANEL] = {19, 22, 29, 255},
                                  [C_RAIL] = {16, 19, 25, 255},
                                  [C_RAISED] = {28, 32, 41, 255},
                                  [C_BORDER] = {51, 62, 79, 255},
                                  [C_MUTED] = {166, 178, 198, 255},
                                  [C_DIM] = {139, 153, 174, 255},
                                  [C_TEXT] = {223, 230, 240, 255},
                                  [C_WHITE] = {245, 248, 252, 255},
                                  [C_ACCENT] = {164, 245, 192, 255},
                                  [C_KEYWORD] = {196, 171, 243, 255},
                                  [C_STRING] = {169, 214, 174, 255},
                                  [C_NUMBER] = {233, 193, 141, 255},
                                  [C_CONSTANT] = {240, 204, 160, 255},
                                  [C_DEFINITION] = {157, 223, 218, 255},
                                  [C_BUILTIN] = {142, 190, 219, 255},
                                  [C_OPERATOR] = {183, 192, 207, 255},
                                  [C_COMMENT] = {148, 175, 157, 255},
                                  [C_RED] = {237, 155, 159, 255},
                                  [C_BLUE] = {138, 203, 224, 255},
                                  [C_LAVENDER] = {196, 171, 243, 255},
                                  [C_BLACK] = {0, 0, 0, 255},
                                  [C_STATUS] = {21, 29, 27, 255}};
Color ui_alpha(Tone t, int a) {
    Color c = ui_colors[t];
    c.a = (unsigned char)(a < 0 ? 0 : a > 255 ? 255 : a);
    return c;
}
void ui_box(float x, float y, float w, float h, Tone t, float radius, int a) {
    if (w <= 0 || h <= 0)
        return;
    Rectangle r = {x, y, w, h};
    if (radius > 0)
        DrawRectangleRounded(r, fminf(1, 2 * radius / fminf(w, h)), 12, ui_alpha(t, a));
    else
        DrawRectangleRec(r, ui_alpha(t, a));
}
void ui_outline(float x, float y, float w, float h, Tone t, float radius, int a) {
    if (w <= 0 || h <= 0)
        return;
    Rectangle r = {x, y, w, h};
    if (radius > 0)
        DrawRectangleRoundedLinesEx(r, fminf(1, 2 * radius / fminf(w, h)), 12, 1, ui_alpha(t, a));
    else
        DrawRectangleLinesEx(r, 1, ui_alpha(t, a));
}
void ui_line(float x, float y, float x2, float y2, Tone t, float thick, int a) {
    DrawLineEx((Vector2){x, y}, (Vector2){x2, y2}, thick, ui_alpha(t, a));
}
bool ui_inside(Rectangle r, Vector2 p) {
    return CheckCollisionPointRec(p, r);
}
int ui_font_codepoints(int *points, int capacity) {
    if (!points || capacity < 409)
        return 0;
    int n = 0;
    for (int c = 32; c < 384; c++)
        points[n++] = c;
    for (int c = 0x2010; c < 0x203b; c++)
        points[n++] = c;
    for (int c = 0x2190; c < 0x219a; c++)
        points[n++] = c;
    points[n++] = 0x2713;
    points[n++] = 0x2715;
    points[n++] = 0x2726;
    points[n++] = 0x25a0;
    return n;
}
bool ui_init(UI *u, const char *assets) {
    memset(u, 0, sizeof(*u));
    int points[1024], n = ui_font_codepoints(points, 1024);
    char path[4096];
    snprintf(path, sizeof(path), "%s/fonts/Inter.ttf", assets);
    if (!FileExists(path))
        return false;
    u->font = LoadFontEx(path, 34, points, n);
    snprintf(path, sizeof(path), "%s/fonts/JetBrainsMono-Medium.ttf", assets);
    if (!FileExists(path))
        snprintf(path, sizeof(path), "%s/fonts/JetBrainsMono-Regular.ttf", assets);
    if (!FileExists(path)) {
        if (IsFontValid(u->font))
            UnloadFont(u->font);
        return false;
    }
    u->mono = LoadFontEx(path, 40, points, n);
    if (!IsFontValid(u->font) || !IsFontValid(u->mono))
        return false;
    SetTextureFilter(u->font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(u->mono.texture, TEXTURE_FILTER_BILINEAR);
    return true;
}
void ui_free(UI *u) {
    UnloadFont(u->font);
    UnloadFont(u->mono);
}
void ui_begin(UI *u) {
    u->hit_count = 0;
    u->layer = 0;
    u->mouse = GetMousePosition();
}
int ui_code_font_size(int saved, bool legacy) {
    if (saved < 12 || saved > UI_CODE_MAX)
        return UI_CODE_DEFAULT;
    int minimum = legacy ? UI_CODE_DEFAULT : UI_CODE_MIN;
    return saved < minimum ? minimum : saved;
}
float ui_text_size(float requested) {
    return fmaxf(UI_TEXT_MIN, roundf(requested));
}
float ui_label_size(float requested) {
    return fmaxf(UI_LABEL_MIN, roundf(requested));
}
void ui_text(UI *u, const char *s, float x, float y, float size, Tone t, bool mono) {
    DrawTextEx(mono ? u->mono : u->font, s, (Vector2){roundf(x), roundf(y)}, ui_text_size(size), 0,
               ui_colors[t]);
}
float ui_width(UI *u, const char *s, float size, bool mono) {
    return MeasureTextEx(mono ? u->mono : u->font, s, ui_text_size(size), 0).x;
}
void ui_label(UI *u, const char *s, float x, float y, float size, Tone t) {
    DrawTextEx(u->font, s, (Vector2){roundf(x), roundf(y)}, ui_label_size(size), 0.6f, ui_colors[t]);
}
size_t ui_elide(UI *u, const char *s, float width, float size, bool mono, char *out, size_t capacity) {
    if (!capacity)
        return 0;
    out[0] = 0;
    if (!s || width <= 0)
        return 0;
    size_t length = strlen(s);
    if (length < capacity && ui_width(u, s, size, mono) <= width) {
        memcpy(out, s, length + 1);
        return length;
    }
    const char *ellipsis = "…";
    float used = 0, tail = ui_width(u, ellipsis, size, mono);
    if (tail > width || capacity < 4)
        return 0;
    size_t n = 0;
    while (s[n] && n < length) {
        int bytes = 0;
        GetCodepointNext(s + n, &bytes);
        if (bytes < 1 || (size_t)bytes > length - n || n + (size_t)bytes + 4 > capacity)
            break;
        char glyph[8] = {0};
        memcpy(glyph, s + n, (size_t)bytes);
        float advance = ui_width(u, glyph, size, mono);
        if (s[n] == '\n' || s[n] == '\r' || used + advance + tail > width)
            break;
        memcpy(out + n, glyph, (size_t)bytes);
        n += (size_t)bytes;
        used += advance;
    }
    memcpy(out + n, ellipsis, 4);
    return n + 3;
}
void ui_text_fit(UI *u, const char *s, float x, float y, float width, float size, Tone t, bool mono) {
    char caption[4096];
    ui_elide(u, s, width, size, mono, caption, sizeof(caption));
    if (*caption)
        ui_text(u, caption, x, y, size, t, mono);
}
static void il(float x, float y, float s, float a, float b, float c, float d, Tone t, int alpha) {
    ui_line(x + a * s, y + b * s, x + c * s, y + d * s, t, 1.7f * s, alpha);
}
static void ic(float x, float y, float s, float a, float b, float r, Tone t, int alpha, bool solid) {
    if (solid)
        DrawCircleV((Vector2){x + a * s, y + b * s}, r * s, ui_alpha(t, alpha));
    else
        DrawCircleLinesV((Vector2){x + a * s, y + b * s}, r * s, ui_alpha(t, alpha));
}
void ui_icon(UI *u, Icon icon, float x, float y, float size, Tone t, int alpha) {
    (void)u;
    float s = size / 24;
#define L(a, b, c, d) il(x, y, s, a, b, c, d, t, alpha)
#define O(a, b, r) ic(x, y, s, a, b, r, t, alpha, false)
#define D(a, b, r) ic(x, y, s, a, b, r, t, alpha, true)
    switch (icon) {
    case I_PRISM:
        L(12, 2, 22, 7);
        L(22, 7, 22, 17);
        L(22, 17, 12, 22);
        L(12, 22, 2, 17);
        L(2, 17, 2, 7);
        L(2, 7, 12, 2);
        L(12, 2, 12, 12);
        L(12, 12, 22, 17);
        L(2, 7, 12, 12);
        L(12, 12, 12, 22);
        break;
    case I_FILES:
        L(8, 3, 19, 3);
        L(19, 3, 19, 17);
        L(19, 17, 8, 17);
        L(8, 17, 8, 3);
        L(4, 7, 4, 21);
        L(4, 21, 15, 21);
        break;
    case I_FILE:
        L(6, 3, 14, 3);
        L(14, 3, 19, 8);
        L(19, 8, 19, 21);
        L(19, 21, 6, 21);
        L(6, 21, 6, 3);
        L(14, 3, 14, 8);
        L(14, 8, 19, 8);
        L(9, 12, 16, 12);
        L(9, 16, 15, 16);
        break;
    case I_FOLDER:
    case I_FOLDER_OPEN:
        L(3, 6, 10, 6);
        L(10, 6, 12, 8);
        L(12, 8, 21, 8);
        L(21, 8, 21, 20);
        L(21, 20, 3, 20);
        L(3, 20, 3, 6);
        if (icon == I_FOLDER_OPEN) {
            L(3, 11, 23, 11);
            L(23, 11, 19, 20);
        }
        break;
    case I_SEARCH:
        O(10, 10, 6);
        L(15, 15, 21, 21);
        break;
    case I_PLAY:
        DrawTriangle((Vector2){x + 6 * s, y + 3 * s}, (Vector2){x + 6 * s, y + 21 * s},
                     (Vector2){x + 21 * s, y + 12 * s}, ui_alpha(t, alpha));
        break;
    case I_STOP:
        ui_box(x + 5 * s, y + 5 * s, 14 * s, 14 * s, t, 2, alpha);
        break;
    case I_SPARK:
        L(12, 2, 14.5, 9.5);
        L(14.5, 9.5, 22, 12);
        L(22, 12, 14.5, 14.5);
        L(14.5, 14.5, 12, 22);
        L(12, 22, 9.5, 14.5);
        L(9.5, 14.5, 2, 12);
        L(2, 12, 9.5, 9.5);
        L(9.5, 9.5, 12, 2);
        break;
    case I_GEAR:
        for (int i = 0; i < 8; i++) {
            float a = (float)i * PI / 4;
            L(12 + cosf(a) * 7, 12 + sinf(a) * 7, 12 + cosf(a) * 10, 12 + sinf(a) * 10);
        }
        O(12, 12, 7);
        O(12, 12, 2.5);
        break;
    case I_CODE:
        L(7, 6, 2, 12);
        L(2, 12, 7, 18);
        L(17, 6, 22, 12);
        L(22, 12, 17, 18);
        L(14, 3, 10, 21);
        break;
    case I_PLUS:
        L(12, 5, 12, 19);
        L(5, 12, 19, 12);
        break;
    case I_MINUS:
        L(5, 12, 19, 12);
        break;
    case I_CLOSE:
        L(6, 6, 18, 18);
        L(18, 6, 6, 18);
        break;
    case I_DOWN:
        L(6, 9, 12, 15);
        L(12, 15, 18, 9);
        break;
    case I_UP:
        L(6, 15, 12, 9);
        L(12, 9, 18, 15);
        break;
    case I_LEFT:
        L(15, 6, 9, 12);
        L(9, 12, 15, 18);
        break;
    case I_RIGHT:
        L(9, 6, 15, 12);
        L(15, 12, 9, 18);
        break;
    case I_MORE:
        D(5, 12, 1.3);
        D(12, 12, 1.3);
        D(19, 12, 1.3);
        break;
    case I_SPLIT:
        L(3, 4, 21, 4);
        L(21, 4, 21, 20);
        L(21, 20, 3, 20);
        L(3, 20, 3, 4);
        L(12, 4, 12, 20);
        break;
    case I_TRASH:
        L(4, 6, 20, 6);
        L(6, 6, 7, 21);
        L(7, 21, 17, 21);
        L(17, 21, 18, 6);
        L(9, 6, 9, 3);
        L(9, 3, 15, 3);
        L(15, 3, 15, 6);
        L(10, 10, 10, 17);
        L(14, 10, 14, 17);
        break;
    case I_SAVE:
        L(4, 3, 17, 3);
        L(17, 3, 21, 7);
        L(21, 7, 21, 21);
        L(21, 21, 4, 21);
        L(4, 21, 4, 3);
        L(8, 3, 8, 10);
        L(8, 10, 16, 10);
        L(16, 10, 16, 3);
        L(8, 21, 8, 15);
        L(8, 15, 17, 15);
        L(17, 15, 17, 21);
        break;
    case I_CHECK:
        L(4, 12, 9, 17);
        L(9, 17, 20, 6);
        break;
    case I_WARNING:
        L(12, 3, 22, 21);
        L(22, 21, 2, 21);
        L(2, 21, 12, 3);
        L(12, 9, 12, 14);
        D(12, 17.5, 0.7);
        break;
    case I_EXPAND:
        L(4, 9, 4, 4);
        L(4, 4, 9, 4);
        L(15, 4, 20, 4);
        L(20, 4, 20, 9);
        L(20, 15, 20, 20);
        L(20, 20, 15, 20);
        L(9, 20, 4, 20);
        L(4, 20, 4, 15);
        break;
    case I_BOOK:
        L(12, 6, 7, 3);
        L(7, 3, 2, 3);
        L(2, 3, 2, 19);
        L(2, 19, 7, 19);
        L(7, 19, 12, 22);
        L(12, 22, 17, 19);
        L(17, 19, 22, 19);
        L(22, 19, 22, 3);
        L(22, 3, 17, 3);
        L(17, 3, 12, 6);
        L(12, 6, 12, 22);
        break;
    case I_TERMINAL:
        L(3, 3, 21, 3);
        L(21, 3, 21, 21);
        L(21, 21, 3, 21);
        L(3, 21, 3, 3);
        L(7, 8, 11, 12);
        L(11, 12, 7, 16);
        L(13, 16, 18, 16);
        break;
    case I_PYTHON:
        ui_box(x + 3 * s, y + 3 * s, 13 * s, 11 * s, C_BLUE, 3, alpha);
        ui_box(x + 8 * s, y + 10 * s, 13 * s, 11 * s, C_NUMBER, 3, alpha);
        ui_box(x + 7 * s, y + 10 * s, 9 * s, 3 * s, C_PANEL, 0, 255);
        DrawCircleV((Vector2){x + 11 * s, y + 6 * s}, 0.8f * s, ui_colors[C_BG]);
        DrawCircleV((Vector2){x + 13 * s, y + 18 * s}, 0.8f * s, ui_colors[C_BG]);
        break;
    }
#undef L
#undef O
#undef D
}
bool ui_hit(UI *u, Rectangle r, int action, int arg, const char *tip) {
    if (u->hit_count < 512)
        u->hits[u->hit_count++] = (Hit){r, action, arg, u->layer, tip};
    return ui_inside(r, u->mouse);
}
void ui_button(UI *u, Rectangle r, int action, int arg, const char *text, int icon, int style, bool active,
               const char *tip) {
    bool hover = ui_hit(u, r, action, arg, tip);
    Tone fg = hover ? C_WHITE : C_MUTED;
    if (style == 2) {
        ui_box(r.x, r.y, r.width, r.height, C_ACCENT, 6, 255);
        fg = C_STATUS;
    } else if (style == 1) {
        ui_box(r.x, r.y, r.width, r.height, hover ? C_RAISED : C_PANEL, 6, 255);
        ui_outline(r.x, r.y, r.width, r.height, hover ? C_DIM : C_BORDER, 6, 255);
        fg = C_TEXT;
    } else if (style == 3) {
        ui_box(r.x, r.y, r.width, r.height, C_RED, 6, hover ? 30 : 18);
        ui_outline(r.x, r.y, r.width, r.height, C_RED, 6, 60);
        fg = C_RED;
    } else if (hover || active) {
        ui_box(r.x, r.y, r.width, r.height, active ? C_ACCENT : C_RAISED, 5, active ? 14 : 255);
        if (active)
            fg = C_ACCENT;
    }
    if (text && *text) {
        float fs = ui_text_size(12);
        char caption[256];
        ui_elide(u, text, r.width - (icon >= 0 ? 24 : 0) - 4, 12, false, caption, sizeof(caption));
        float tw = ui_width(u, caption, 12, false), start = r.x + (r.width - tw - (icon >= 0 ? 24 : 0)) / 2;
        if (icon >= 0)
            ui_icon(u, (Icon)icon, start, r.y + (r.height - 16) / 2, 16, fg, 255);
        ui_text(u, caption, start + (icon >= 0 ? 24 : 0), r.y + (r.height - fs) / 2 - 1, 12, fg, false);
    } else if (icon >= 0)
        ui_icon(u, (Icon)icon, r.x + (r.width - 18) / 2, r.y + (r.height - 18) / 2, 18, fg, 255);
}
const Hit *ui_clicked(UI *u, bool modal) {
    for (int i = u->hit_count - 1; i >= 0; i--)
        if (u->hits[i].layer == (int)modal && ui_inside(u->hits[i].rect, GetMousePosition()))
            return &u->hits[i];
    return NULL;
}
void ui_tooltip(UI *u, bool modal) {
    const Hit *h = ui_clicked(u, modal);
    const char *tip = h ? h->tip : NULL;
    if (tip != u->tip) {
        u->tip = tip;
        u->tip_since = GetTime();
    }
    if (tip && *tip && GetTime() - u->tip_since > 0.65) {
        float w = fminf((float)GetScreenWidth() - 16, ui_width(u, tip, 11, false) + 24),
              x = fmaxf(8, fminf((float)GetScreenWidth() - w - 8, u->mouse.x - w / 2));
        float y = fminf((float)GetScreenHeight() - 42, u->mouse.y + 22);
        ui_box(x, y, w, 36, C_RAISED, 5, 255);
        ui_outline(x, y, w, 36, C_DIM, 5, 255);
        ui_text_fit(u, tip, x + 12, y + 9, w - 24, 11, C_WHITE, false);
    }
}
