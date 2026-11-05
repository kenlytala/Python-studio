#ifndef PRISME_UI_H
#define PRISME_UI_H
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    C_BG,
    C_PANEL,
    C_RAIL,
    C_RAISED,
    C_BORDER,
    C_MUTED,
    C_DIM,
    C_TEXT,
    C_WHITE,
    C_ACCENT,
    C_KEYWORD,
    C_STRING,
    C_NUMBER,
    C_CONSTANT,
    C_DEFINITION,
    C_BUILTIN,
    C_OPERATOR,
    C_COMMENT,
    C_RED,
    C_BLUE,
    C_LAVENDER,
    C_BLACK,
    C_STATUS,
    C_COUNT
} Tone;
extern const Color ui_colors[C_COUNT];
enum { UI_TEXT_MIN = 17, UI_LABEL_MIN = 15, UI_CODE_DEFAULT = 20, UI_CODE_MIN = 17, UI_CODE_MAX = 32 };
float ui_text_size(float requested);
float ui_label_size(float requested);
int ui_code_font_size(int saved, bool legacy);
/* Fit a UTF-8 caption with an ellipsis, never by shrinking it. */

typedef enum {
    I_PRISM,
    I_FILES,
    I_FILE,
    I_FOLDER,
    I_FOLDER_OPEN,
    I_SEARCH,
    I_CODE,
    I_PLAY,
    I_STOP,
    I_SPARK,
    I_GEAR,
    I_BOOK,
    I_PLUS,
    I_CLOSE,
    I_DOWN,
    I_UP,
    I_LEFT,
    I_RIGHT,
    I_MORE,
    I_SPLIT,
    I_TRASH,
    I_EXPAND,
    I_SAVE,
    I_CHECK,
    I_WARNING,
    I_PYTHON,
    I_MINUS,
    I_TERMINAL
} Icon;
typedef struct {
    Rectangle rect;
    int action, arg, layer;
    const char *tip;
} Hit;
typedef struct {
    Font font, mono;
    Vector2 mouse;
    Hit hits[512];
    int hit_count, layer;
    const char *tip;
    double tip_since;
} UI;

Color ui_alpha(Tone tone, int alpha);
void ui_box(float x, float y, float w, float h, Tone tone, float radius, int alpha);
void ui_outline(float x, float y, float w, float h, Tone tone, float radius, int alpha);
void ui_line(float x, float y, float x2, float y2, Tone tone, float thick, int alpha);
bool ui_inside(Rectangle r, Vector2 p);
int ui_font_codepoints(int *points, int capacity);
bool ui_init(UI *u, const char *assets);
void ui_free(UI *u);
void ui_begin(UI *u);
void ui_text(UI *u, const char *s, float x, float y, float size, Tone tone, bool mono);
float ui_width(UI *u, const char *s, float size, bool mono);
void ui_text_fit(UI *u, const char *s, float x, float y, float width, float size, Tone tone, bool mono);
/* Pure measurement helper, also usable by the windowless tests. */
size_t ui_elide(UI *u, const char *s, float width, float size, bool mono, char *out, size_t capacity);
void ui_label(UI *u, const char *s, float x, float y, float size, Tone tone);
void ui_icon(UI *u, Icon icon, float x, float y, float size, Tone tone, int alpha);
bool ui_hit(UI *u, Rectangle rect, int action, int arg, const char *tip);
void ui_button(UI *u, Rectangle rect, int action, int arg, const char *text, int icon, int style, bool active,
               const char *tip);
const Hit *ui_clicked(UI *u, bool modal);
void ui_tooltip(UI *u, bool modal);
#endif
