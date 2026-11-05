/* CPU-only typography/contrast tests. No window or graphics context is opened. */
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                                  \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static double channel(unsigned char v) {
    double c = v / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
static double luminance(Color c) {
    return .2126 * channel(c.r) + .7152 * channel(c.g) + .0722 * channel(c.b);
}
static double contrast(Tone a, Tone b) {
    double x = luminance(ui_colors[a]), y = luminance(ui_colors[b]);
    return (fmax(x, y) + .05) / (fmin(x, y) + .05);
}
static bool valid_utf8(const char *s) {
    for (size_t n = 0; s[n];) {
        int bytes = 0, code = GetCodepointNext(s + n, &bytes);
        if (bytes <= 0 || (code == 63 && s[n] != '?'))
            return false;
        n += (size_t)bytes;
    }
    return true;
}
static int font_warnings, atlas_fonts, tall_glyphs;
static size_t atlas_pixels;
static void font_log(int level, const char *message, va_list args) {
    if (level >= LOG_WARNING) {
        font_warnings++;
        vfprintf(stderr, message, args);
        fputc('\n', stderr);
    }
}
static bool check_atlas(Font font) {
    Rectangle *rects = NULL;
    Image atlas = GenImageFontAtlas(font.glyphs, &rects, font.glyphCount, font.baseSize, 4, 0);
    bool ok = atlas.data && rects && atlas.width > 0 && atlas.height > 0 &&
              atlas.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
    for (int i = 0; ok && i < font.glyphCount; i++) {
        GlyphInfo g = font.glyphs[i];
        Rectangle r = rects[i];
        if (g.image.height > font.baseSize)
            tall_glyphs++;
        if (!g.image.data || g.image.width <= 0 || g.image.height <= 0)
            continue;
        ok = r.width == g.image.width && r.height == g.image.height && r.x >= 0 && r.y >= 0 &&
             r.x + r.width <= atlas.width && r.y + r.height <= atlas.height;
        for (int y = 0; ok && y < g.image.height; y++)
            for (int x = 0; ok && x < g.image.width; x++) {
                size_t at = ((size_t)((int)r.y + y) * (size_t)atlas.width + (size_t)((int)r.x + x)) * 2u + 1u;
                ok = ((unsigned char *)atlas.data)[at] ==
                     ((unsigned char *)g.image.data)[y * g.image.width + x];
                atlas_pixels++;
            }
        for (int j = 0; ok && j < i; j++) {
            Rectangle other = rects[j];
            if (other.width <= 0 || other.height <= 0)
                continue;
            ok = !(r.x < other.x + other.width && r.x + r.width > other.x && r.y < other.y + other.height &&
                   r.y + r.height > other.y);
        }
    }
    if (atlas.data)
        UnloadImage(atlas);
    if (rects)
        MemFree(rects);
    atlas_fonts++;
    return ok;
}
static Font cpu_font(const char *assets, const char *name, int size) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/fonts/%s", assets, name);
    int length = 0;
    unsigned char *data = LoadFileData(path, &length);
    Font font = {0};
    if (!data)
        return font;
    int points[1024];
    int count = ui_font_codepoints(points, 1024);
    font.baseSize = size;
    font.glyphCount = count;
    font.glyphs = LoadFontData(data, length, size, points, count, FONT_DEFAULT);
    UnloadFileData(data);
    if (!font.glyphs)
        return (Font){0};
    font.recs = MemAlloc(sizeof(Rectangle) * (size_t)count);
    if (!font.recs) {
        UnloadFontData(font.glyphs, count);
        return (Font){0};
    }
    for (int i = 0; i < count; i++)
        font.recs[i] =
            (Rectangle){0, 0, (float)font.glyphs[i].image.width, (float)font.glyphs[i].image.height};
    return font;
}
static void free_cpu_font(Font font) {
    UnloadFontData(font.glyphs, font.glyphCount);
    MemFree(font.recs);
}
int main(int argc, char **argv) {
    const Tone backgrounds[] = {C_BG, C_PANEL, C_RAIL, C_RAISED, C_STATUS};
    const Tone foregrounds[] = {C_TEXT,     C_WHITE,      C_MUTED,   C_DIM,     C_COMMENT,
                                C_ACCENT,   C_RED,        C_KEYWORD, C_STRING,  C_NUMBER,
                                C_CONSTANT, C_DEFINITION, C_BUILTIN, C_OPERATOR};
    double minimum = 100;
    for (unsigned b = 0; b < sizeof(backgrounds) / sizeof(*backgrounds); b++)
        for (unsigned f = 0; f < sizeof(foregrounds) / sizeof(*foregrounds); f++) {
            double c = contrast(foregrounds[f], backgrounds[b]);
            if (c < minimum)
                minimum = c;
            CHECK(c >= 4.5);
        }
    CHECK(contrast(C_STATUS, C_ACCENT) >= 7);
    for (int n = 1; n <= 40; n++) {
        CHECK(ui_text_size((float)n) >= UI_TEXT_MIN);
        CHECK(ui_text_size((float)n) >= (float)n);
        CHECK(ui_label_size((float)n) >= UI_LABEL_MIN);
    }
    CHECK(ui_text_size(UI_CODE_DEFAULT) == 20);
    CHECK(ui_code_font_size(12, true) == 20 && ui_code_font_size(18, true) == 20);
    CHECK(ui_code_font_size(26, true) == 26 && ui_code_font_size(32, true) == 32);
    CHECK(ui_code_font_size(17, false) == 17 && ui_code_font_size(19, false) == 19);
    CHECK(ui_code_font_size(0, false) == 20 && ui_code_font_size(99, false) == 20);
    /* A synthetic font is enough to measure captions through real raylib code. */
    GlyphInfo glyphs[257] = {0};
    Rectangle recs[257] = {0};
    for (int i = 0; i < 257; i++) {
        glyphs[i].value = i == 256 ? 0x2026 : i;
        glyphs[i].advanceX = 12;
        recs[i] = (Rectangle){0, 0, 12, 20};
    }
    Font font = {0};
    font.baseSize = 20;
    font.glyphCount = 257;
    font.glyphs = glyphs;
    font.recs = recs;
    UI ui = {0};
    ui.font = font;
    ui.mono = font;
    const char *caption = "Étoile et caractères accentués 🌟 — fichier.py";
    int fit_cases = 0;
    for (size_t capacity = 0; capacity <= 40; capacity++)
        for (int width = 0; width <= 600; width += 5) {
            char out[64];
            memset(out, 'Z', sizeof(out));
            size_t n = ui_elide(&ui, caption, (float)width, 11, true, out, capacity);
            CHECK(out[capacity] == 'Z');
            if (capacity) {
                CHECK(memchr(out, 0, capacity) != NULL);
                CHECK(strlen(out) == n && valid_utf8(out));
                CHECK(ui_width(&ui, out, 11, true) <= (float)width + .01f);
            } else
                CHECK(n == 0);
            fit_cases++;
        }
    char out[80];
    CHECK(ui_elide(&ui, "abc", 100, 11, false, out, sizeof(out)) == 3 && !strcmp(out, "abc"));
    CHECK(ui_elide(&ui, caption, ui_width(&ui, "É…", 11, true), 11, true, out, sizeof(out)) == 5);
    CHECK(!strcmp(out, "É…"));
    if (argc > 1) {
        SetTraceLogLevel(LOG_WARNING);
        SetTraceLogCallback(font_log);
        int points[410];
        for (int i = 0; i < 410; i++)
            points[i] = -1;
        CHECK(ui_font_codepoints(NULL, 1024) == 0 && ui_font_codepoints(points, 408) == 0 && points[0] == -1);
        CHECK(ui_font_codepoints(points, 409) == 409 && points[409] == -1);
        const char *names[] = {"Inter.ttf", "JetBrainsMono-Medium.ttf", "JetBrainsMono-Regular.ttf"};
        const int sizes[] = {17, 20, 34, 40, 64};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 5; j++) {
                Font f = cpu_font(argv[1], names[i], sizes[j]);
                CHECK(f.glyphs && f.recs);
                CHECK(check_atlas(f));
                if (sizes[j] == 34 || sizes[j] == 40) {
                    int index = GetGlyphIndex(f, '|');
                    printf("Glyph | %s @%d: actual bitmap height %d (nominal %d)\n", names[i], sizes[j],
                           f.glyphs[index].image.height, f.baseSize);
                }
                free_cpu_font(f);
            }
        CHECK(tall_glyphs > 0 && font_warnings == 0);
        printf("OK %d full 409-glyph atlases, %zu original alpha pixels preserved, no overlap/crop/font "
               "warnings; CPU only\n",
               atlas_fonts, atlas_pixels);
        UI real = {0};
        real.font = cpu_font(argv[1], "Inter.ttf", 34);
        real.mono = cpu_font(argv[1], "JetBrainsMono-Medium.ttf", 40);
        CHECK(real.font.glyphs && real.mono.glyphs);
        const char *labels[] = {"Dossiers", "Menu",    "Console", "Tout replier", "Restaurer",
                                "Exécuter", "Arrêter", "Effets",  "Quitter"};
        const float button_widths[] = {100, 80, 96, 130, 130, 112, 112, 93, 104};
        for (int i = 0; i < 9; i++)
            CHECK(ui_width(&real, labels[i], 12, false) + 24 <= button_widths[i] - 4);
        CHECK(ui_width(&real, "Ctrl K", 9, false) <= 46);
        for (int width = 960; width <= 1920; width += 80) {
            const char *info[] = {"60 fps", "Python 3", "Ln 2097153, Col 2097153", "Ln 1, Col 1"};
            float right = width - 8 - 104 - 14;
            if (width >= 1440)
                right -= ui_width(&real, info[0], 12, true) + 16;
            if (width >= 1200)
                right -= ui_width(&real, info[1], 12, false) + 16;
            if (width >= 1600)
                right -= ui_width(&real, "UTF-8", 12, false) + 16;
            if (width >= 1760)
                right -= ui_width(&real, "4 espaces", 12, false) + 16;
            CHECK(right - ui_width(&real, info[2], 12, true) > 500);
            CHECK(right - ui_width(&real, info[3], 12, true) > 500);
        }
        free_cpu_font(real.font);
        free_cpu_font(real.mono);
        CHECK(font_warnings == 0);
        SetTraceLogCallback(NULL);
        puts("OK actual Inter/Mono Medium metrics, full control labels and footer positions (CPU only)");
    }
    printf("OK UI text >= %d px, labels >= %d px, minimum text contrast %.2f:1\n", UI_TEXT_MIN, UI_LABEL_MIN,
           minimum);
    printf("OK %d UTF-8 caption fits; no font shrinking, no GPU, no preview\n", fit_cases);
    return 0;
}
