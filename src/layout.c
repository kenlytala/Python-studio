#include "layout.h"
#include <math.h>

static int min_int(int a, int b) {
    return a < b ? a : b;
}
static int max_int(int a, int b) {
    return a > b ? a : b;
}
static int clamp_int(int n, int low, int high) {
    return high < low ? high : n < low ? low : n > high ? high : n;
}
WorkspaceLayout layout_workspace(int w, int h, bool sidebar, bool topbar, bool console, bool find,
                                 int sidebar_w, int console_h) {
    WorkspaceLayout r = {0};
    r.width = max_int(0, w);
    r.height = max_int(0, h);
    r.status_h = min_int(LAYOUT_STATUS_H, r.height);
    r.status_y = r.height - r.status_h;
    r.topbar_h = topbar ? min_int(LAYOUT_TOPBAR_H, r.status_y) : 0;
    r.tabs_y = r.topbar_h;
    r.crumb_y = min_int(r.status_y, r.tabs_y + LAYOUT_TABS_H);
    r.find_y = min_int(r.status_y, r.crumb_y + LAYOUT_CRUMB_H);
    r.code_y = min_int(r.status_y, r.find_y + (find ? LAYOUT_FIND_H : 0));
    int max_sidebar = min_int(LAYOUT_SIDEBAR_MAX, max_int(0, r.width - 520));
    if (sidebar && max_sidebar >= LAYOUT_SIDEBAR_MIN)
        r.sidebar_w = clamp_int(sidebar_w, LAYOUT_SIDEBAR_MIN, max_sidebar);
    int max_console = max_int(0, r.status_y - r.code_y - LAYOUT_MIN_CODE_H);
    r.console_h = console ? clamp_int(console_h, 110, max_console) : 0;
    r.console_y = r.status_y - r.console_h;
    r.code_bottom = r.console_y;
    r.code_h = r.code_bottom - r.code_y;
    return r;
}
TreeLayout layout_tree(const WorkspaceLayout *r) {
    TreeLayout tree = {0};
    tree.top = min_int(r->status_y, r->topbar_h + LAYOUT_TREE_HEADER_H);
    tree.bottom = max_int(tree.top, r->status_y - LAYOUT_TREE_BOTTOM_PAD);
    tree.first_y = min_int(tree.bottom, tree.top + LAYOUT_TREE_ROW_INSET);
    int available = tree.bottom - tree.first_y;
    if (available >= LAYOUT_TREE_ITEM_H)
        tree.rows = 1 + (available - LAYOUT_TREE_ITEM_H) / LAYOUT_TREE_ROW_H;
    return tree;
}
int layout_tree_rows(const WorkspaceLayout *r) {
    return layout_tree(r).rows;
}
int layout_tree_scroll(const WorkspaceLayout *r, int entry_count, int scroll) {
    int max_scroll = max_int(0, max_int(0, entry_count) - layout_tree_rows(r));
    return clamp_int(scroll, 0, max_scroll);
}

TabBarLayout layout_tabbar(const WorkspaceLayout *layout, int tab_count, int active_tab) {
    TabBarLayout r = {0};
    r.more_x = layout->width - 15 - LAYOUT_TAB_MORE_W;
    r.run_x = r.more_x - 12 - LAYOUT_TAB_RUN_W;
    /* Leave room for +, its inset and a gap before Exécuter / Arrêter. */
    int slots = max_int(1, (r.run_x - 12 - layout->sidebar_w - 8 - LAYOUT_TAB_ADD_W) / LAYOUT_TAB_W);
    tab_count = max_int(0, tab_count);
    active_tab = tab_count ? clamp_int(active_tab, 0, tab_count - 1) : 0;
    r.first = max_int(0, active_tab - slots + 1);
    r.count = min_int(slots, tab_count - r.first);
    r.new_x = layout->sidebar_w + r.count * LAYOUT_TAB_W + 8;
    return r;
}

int layout_quit_x(int width) {
    // Compatibilité : ancien bouton large "Quitter" avec texte
    return max_int(0, width - 47 - LAYOUT_QUIT_W);
}
int layout_window_group_w(void) {
    return 3 * LAYOUT_WINDOW_W + 2 * LAYOUT_WINDOW_GAP;
}
int layout_window_x(int width, int index) {
    // Groupe aligné à 6px du bord droit : [Plein écran][gap][Réduire][gap][Quitter]
    int group = layout_window_group_w();
    int start = max_int(0, width - 6 - group);
    if (index < 0) index = 0;
    if (index > 2) index = 2;
    return start + index * (LAYOUT_WINDOW_W + LAYOUT_WINDOW_GAP);
}
PopupLayout layout_context(int width, int height, int status_y, int mouse_x, int mouse_y) {
    PopupLayout r = {0};
    int bottom = min_int(height, status_y);
    r.width = min_int(LAYOUT_CONTEXT_W, max_int(0, width - 16));
    r.height = min_int(LAYOUT_CONTEXT_H, max_int(0, bottom - 16));
    r.x = clamp_int(mouse_x, 8, max_int(8, width - r.width - 8));
    r.y = clamp_int(mouse_y, 8, max_int(8, bottom - r.height - 8));
    return r;
}

ScrollAxis layout_scroll_axis(float start, float span, unsigned visible, unsigned total, unsigned position) {
    if (span < 0)
        span = 0;
    if (!visible)
        visible = 1;
    if (!total)
        total = 1;
    unsigned limit = total > visible ? total - visible : 0;
    float minimum = span / 2 < 28 ? span / 2 : 28;
    float thumb = span * (float)visible / (float)total;
    if (thumb < minimum)
        thumb = minimum;
    if (thumb > span || !limit)
        thumb = span;
    if (position > limit)
        position = limit;
    float offset = limit ? (span - thumb) * (float)position / (float)limit : 0;
    return (ScrollAxis){start, span, start + offset, thumb, limit};
}
unsigned layout_scroll_position(ScrollAxis axis, float pointer, float grab) {
    float travel = axis.span - axis.thumb_span;
    if (!axis.limit || travel <= 0 || !isfinite(pointer))
        return 0;
    double fraction = ((double)pointer - axis.start - grab) / travel;
    if (fraction <= 0)
        return 0;
    if (fraction >= 1)
        return axis.limit;
    return (unsigned)(fraction * (double)axis.limit + .5);
}
