#ifndef PRISME_LAYOUT_H
#define PRISME_LAYOUT_H
#include <stdbool.h>

enum {
    LAYOUT_STATUS_H = 36,
    LAYOUT_TOPBAR_H = 69,
    LAYOUT_TABS_H = 47,
    LAYOUT_TAB_W = 173,
    LAYOUT_TAB_ADD_W = 27,
    LAYOUT_TAB_RUN_W = 112,
    LAYOUT_TAB_MORE_W = 29,
    LAYOUT_CRUMB_H = 35,
    LAYOUT_FIND_H = 43,
    LAYOUT_MIN_CODE_H = 140,
    LAYOUT_SIDEBAR_DEFAULT = 226,
    LAYOUT_SIDEBAR_MIN = 200,
    LAYOUT_SIDEBAR_MAX = 420,
    LAYOUT_TREE_HEADER_H = 100,
    LAYOUT_TREE_ROW_INSET = 3,
    LAYOUT_TREE_ROW_H = 33,
    LAYOUT_TREE_ITEM_H = 31,
    LAYOUT_TREE_BOTTOM_PAD = 8,
    LAYOUT_QUIT_W = 104,
    LAYOUT_WINDOW_W = 32,
    LAYOUT_WINDOW_GAP = 6,
    LAYOUT_CONTEXT_W = 304,
    LAYOUT_CONTEXT_H = 242,
    LAYOUT_CONTEXT_ROW_H = 36
};
typedef struct {
    int width, height;
    int topbar_h, tabs_y, crumb_y, find_y;
    int sidebar_w, code_y, code_bottom, code_h;
    int console_y, console_h, status_y, status_h;
} WorkspaceLayout;
typedef struct {
    int top, bottom, first_y, rows;
} TreeLayout;
typedef struct {
    int first, count;
    int new_x, run_x, more_x;
} TabBarLayout;
typedef struct {
    int x, y, width, height;
} PopupLayout;
typedef struct {
    float start, span, thumb_start, thumb_span;
    unsigned limit;
} ScrollAxis;
/* Pure one-axis scrollbar geometry, used by the read-only search viewport. */
ScrollAxis layout_scroll_axis(float start, float span, unsigned visible, unsigned total, unsigned position);
unsigned layout_scroll_position(ScrollAxis axis, float pointer, float grab);
PopupLayout layout_context(int width, int height, int status_y, int mouse_x, int mouse_y);
/* Groupe fenêtres à l'extrême droite : Plein écran | Réduire | Quitter (sans texte, icônes seules). */
int layout_quit_x(int width);
/* Positions du groupe fenêtre (0=Plein écran, 1=Réduire, 2=Quitter) */
int layout_window_x(int width, int index);
int layout_window_group_w(void);
/* Reserve the run/menu controls before deciding how many file tabs fit. */
TabBarLayout layout_tabbar(const WorkspaceLayout *layout, int tab_count, int active_tab);
/* Pure geometry: no GPU/window needed, so every panel combination is testable. */
WorkspaceLayout layout_workspace(int width, int height, bool sidebar, bool topbar, bool console, bool find,
                                 int sidebar_w, int console_h);
/* The file list occupies all space below its header, down to the status bar. */
TreeLayout layout_tree(const WorkspaceLayout *layout);
int layout_tree_rows(const WorkspaceLayout *layout);
int layout_tree_scroll(const WorkspaceLayout *layout, int entry_count, int scroll);
#endif
