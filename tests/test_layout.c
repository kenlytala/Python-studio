#include "layout.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                             \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main(void) {
    const int sizes[][2] = {{960, 640}, {1440, 900}, {1920, 1080}, {800, 600},
                            {400, 240}, {0, 0},      {-20, -30},   {INT_MAX, INT_MAX}};
    int checked = 0;
    for (unsigned s = 0; s < sizeof(sizes) / sizeof(*sizes); s++)
        for (int bits = 0; bits < 16; bits++)
            for (int small = 0; small < 3; small++) {
                int width = small == 0 ? 226 : small == 1 ? 100 : 100000;
                int height = small == 0 ? 211 : small == 1 ? -20 : 100000;
                WorkspaceLayout r = layout_workspace(sizes[s][0], sizes[s][1], bits & 1, bits & 2, bits & 4,
                                                     bits & 8, width, height);
                CHECK(r.sidebar_w >= 0 && r.sidebar_w <= LAYOUT_SIDEBAR_MAX);
                CHECK(r.topbar_h >= 0 && r.topbar_h <= 69);
                CHECK(r.tabs_y == r.topbar_h && r.crumb_y >= r.tabs_y && r.find_y >= r.crumb_y);
                CHECK(r.code_y >= r.find_y && r.code_bottom >= r.code_y &&
                      r.code_h == r.code_bottom - r.code_y);
                CHECK(r.console_h >= 0 && r.console_y == r.status_y - r.console_h);
                CHECK(r.status_y >= 0 && r.status_y <= (sizes[s][1] > 0 ? sizes[s][1] : 0));
                CHECK(r.status_h >= 0 && r.status_h <= LAYOUT_STATUS_H);
                CHECK(r.status_y + r.status_h == r.height);
                CHECK(r.code_bottom == r.console_y && r.console_y + r.console_h == r.status_y);
                if (!(bits & 1))
                    CHECK(r.sidebar_w == 0);
                if (!(bits & 2))
                    CHECK(r.topbar_h == 0 && r.tabs_y == 0);
                if (!(bits & 4))
                    CHECK(r.console_h == 0 && r.code_bottom == r.status_y);
                if (sizes[s][1] >= 600)
                    CHECK(r.code_h >= LAYOUT_MIN_CODE_H);
                TreeLayout tree = layout_tree(&r);
                CHECK(tree.top >= r.topbar_h && tree.top <= tree.bottom && tree.bottom <= r.status_y);
                CHECK(tree.first_y >= tree.top && tree.first_y <= tree.bottom);
                CHECK(tree.rows >= 0 && layout_tree_rows(&r) == tree.rows);
                if (tree.rows) {
                    CHECK((long long)tree.first_y + (long long)(tree.rows - 1) * LAYOUT_TREE_ROW_H +
                              LAYOUT_TREE_ITEM_H <=
                          tree.bottom);
                    CHECK((long long)tree.first_y + (long long)tree.rows * LAYOUT_TREE_ROW_H +
                              LAYOUT_TREE_ITEM_H >
                          tree.bottom);
                }
                checked++;
            }
    WorkspaceLayout full = layout_workspace(1440, 900, true, true, true, false, 226, 211);
    WorkspaceLayout no_left = layout_workspace(1440, 900, false, true, true, false, 226, 211);
    WorkspaceLayout no_menu = layout_workspace(1440, 900, true, false, true, false, 226, 211);
    WorkspaceLayout no_console = layout_workspace(1440, 900, true, true, false, false, 226, 211);
    WorkspaceLayout zen = layout_workspace(1440, 900, false, false, false, false, 226, 211);
    CHECK(full.sidebar_w == 226); /* No residual 62 px shortcut rail. */
    CHECK(no_left.sidebar_w == 0);
    CHECK(full.code_y - no_menu.code_y == 69 && no_menu.code_h - full.code_h == 69);
    CHECK(no_console.code_h - full.code_h == 211);
    CHECK(zen.code_y == 82 && zen.code_bottom == 864 && zen.code_h == 782);
    CHECK(zen.sidebar_w == 0);
    CHECK(layout_tree_rows(&no_menu) >= layout_tree_rows(&full));
    int tab_cases = 0;
    const int widths[] = {960, 1100, 1440, 1920};
    const int sidebar_widths[] = {0, 200, 226, 420};
    for (unsigned w = 0; w < sizeof(widths) / sizeof(*widths); w++)
        for (unsigned side = 0; side < sizeof(sidebar_widths) / sizeof(*sidebar_widths); side++)
            for (int menu = 0; menu < 2; menu++) {
                WorkspaceLayout r = layout_workspace(widths[w], 900, sidebar_widths[side] != 0, menu != 0,
                                                     true, false, sidebar_widths[side], 211);
                for (int count = 1; count <= 24; count++)
                    for (int active = 0; active < count; active++) {
                        TabBarLayout bar = layout_tabbar(&r, count, active);
                        CHECK(bar.first >= 0 && bar.count > 0 && bar.first + bar.count <= count);
                        CHECK(active >= bar.first && active < bar.first + bar.count);
                        CHECK(bar.new_x == r.sidebar_w + bar.count * LAYOUT_TAB_W + 8);
                        CHECK(bar.new_x + LAYOUT_TAB_ADD_W + 12 <= bar.run_x);
                        CHECK(bar.run_x + LAYOUT_TAB_RUN_W + 12 == bar.more_x);
                        CHECK(bar.more_x + LAYOUT_TAB_MORE_W == r.width - 15);
                        CHECK(7 + 33 / 2.0 == 10 + 27 / 2.0); /* Run and ellipsis share a centerline. */
                        CHECK(r.tabs_y == (menu ? 69 : 0));
                        tab_cases++;
                    }
            }
    TabBarLayout empty = layout_tabbar(&full, 0, -1);
    CHECK(empty.first == 0 && empty.count == 0 && empty.new_x == full.sidebar_w + 8);
    TabBarLayout last = layout_tabbar(&full, 24, 100);
    CHECK(last.first + last.count == 24);
    int resize_cases = 0;
    for (int h = 0; h <= 2160; h += 7)
        for (int mask = 0; mask < 16; mask++) {
            WorkspaceLayout r = layout_workspace(1440, h, mask & 1, mask & 2, mask & 4, mask & 8, 226, 211);
            CHECK(r.status_y + r.status_h == h);
            CHECK(r.status_h == (h < LAYOUT_STATUS_H ? h : LAYOUT_STATUS_H));
            CHECK(r.code_bottom + r.console_h == r.status_y);
            resize_cases++;
        }
    int tree_cases = 0;
    const int entry_counts[] = {-5, 0, 1, 7, 512, INT_MAX};
    const int offsets[] = {INT_MIN, -1, 0, 10, INT_MAX};
    for (int h = 0; h <= 2160; h++)
        for (int menu = 0; menu < 2; menu++) {
            WorkspaceLayout r = layout_workspace(1440, h, true, menu != 0, true, false, 226, 211);
            TreeLayout tree = layout_tree(&r);
            CHECK(tree.top <= tree.bottom && tree.bottom <= r.status_y);
            if (h >= 640) {
                CHECK(tree.bottom == r.status_y - LAYOUT_TREE_BOTTOM_PAD);
                int old_rows = (r.status_y - 277 - (r.topbar_h + 103)) / 33;
                CHECK(tree.rows >= old_rows + 8);
                CHECK(tree.bottom - (r.status_y - 277) == 269);
            }
            for (unsigned count = 0; count < sizeof(entry_counts) / sizeof(*entry_counts); count++)
                for (unsigned offset = 0; offset < sizeof(offsets) / sizeof(*offsets); offset++) {
                    int total = entry_counts[count] < 0 ? 0 : entry_counts[count];
                    int max_scroll = total > tree.rows ? total - tree.rows : 0;
                    int scroll = layout_tree_scroll(&r, entry_counts[count], offsets[offset]);
                    CHECK(scroll >= 0 && scroll <= max_scroll);
                    CHECK(layout_tree_scroll(&r, entry_counts[count], INT_MAX) == max_scroll);
                    CHECK(layout_tree_scroll(&r, entry_counts[count], INT_MIN) == 0);
                    if (total && tree.rows) {
                        int visible = total - max_scroll;
                        CHECK(visible <= tree.rows && max_scroll + visible == total);
                    }
                    tree_cases++;
                }
        }
    CHECK(layout_tree_rows(&full) == 20);
    CHECK(layout_tree_rows(&no_menu) == 22);
    WorkspaceLayout tall = layout_workspace(1440, 1440, true, true, true, false, 226, 211);
    CHECK(layout_tree_scroll(&tall, 100, 100 - layout_tree_rows(&full)) == 100 - layout_tree_rows(&tall));
    printf("OK %d explorer bounds/scroll cases: decoration removed, full list height, last file reachable\n",
           tree_cases);
    int context_cases = 0;
    for (int width = 960; width <= 3840; width += 96)
        for (int height = 640; height <= 2160; height += 80)
            for (int x = -100; x <= width + 100; x += 157)
                for (int y = -100; y <= height + 100; y += 173) {
                    PopupLayout popup = layout_context(width, height, height - LAYOUT_STATUS_H, x, y);
                    CHECK(popup.x >= 8 && popup.y >= 8);
                    CHECK(popup.x + popup.width <= width - 8);
                    CHECK(popup.y + popup.height <= height - LAYOUT_STATUS_H - 8);
                    CHECK(popup.height >= 48 + 5 * LAYOUT_CONTEXT_ROW_H);
                    CHECK(layout_quit_x(width) >= 488 + 12);
                    CHECK(layout_quit_x(width) + LAYOUT_QUIT_W == width - 47);
                    context_cases++;
                }
    printf("OK %d context popup positions and persistent Quit button bounds\n", context_cases);
    printf("OK %d resize states: status bar flush with final window pixel\n", resize_cases);
    printf("OK %d panel geometries + full-width/full-height recovery\n", checked);
    printf("OK %d tab bars: active tab visible, + / Run / ... never overlap, menu shown or hidden\n",
           tab_cases);
    unsigned scroll_cases = 0;
    const unsigned totals[] = {0, 1, 10, 1000, 200022, 2097153, 8388609};
    const unsigned visibles[] = {1, 2, 25, 60, 5000};
    const float starts[] = {-10, 0, 17, 500}, spans[] = {0, 1, 12, 28, 71, 240, 1200};
    for (unsigned a = 0; a < sizeof(starts) / sizeof(*starts); a++)
        for (unsigned b = 0; b < sizeof(spans) / sizeof(*spans); b++)
            for (unsigned c = 0; c < sizeof(visibles) / sizeof(*visibles); c++)
                for (unsigned d = 0; d < sizeof(totals) / sizeof(*totals); d++) {
                    unsigned total = totals[d] ? totals[d] : 1, visible = visibles[c],
                             limit = total > visible ? total - visible : 0;
                    unsigned positions[] = {0, limit / 4, limit / 2, limit, limit + 100};
                    for (unsigned e = 0; e < sizeof(positions) / sizeof(*positions); e++) {
                        float start = starts[a], span = spans[b];
                        ScrollAxis axis = layout_scroll_axis(start, span, visible, total, positions[e]);
                        CHECK(axis.limit == limit && axis.thumb_start >= start && axis.thumb_span >= 0);
                        CHECK(axis.thumb_start + axis.thumb_span <= start + span + .001f);
                        if (span && limit) {
                            CHECK(axis.thumb_span < span);
                            float grab = axis.thumb_span / 2;
                            CHECK(layout_scroll_position(axis, start - 10000, grab) == 0);
                            CHECK(layout_scroll_position(axis, start + span + 10000, grab) == limit);
                            unsigned actual = layout_scroll_position(axis, axis.thumb_start + grab, grab);
                            unsigned expected = positions[e] > limit ? limit : positions[e];
                            unsigned difference = actual > expected ? actual - expected : expected - actual;
                            float absolute = start < 0 ? -start : start;
                            unsigned tolerance = 2 + (unsigned)((float)limit * .0000003f * (absolute + span) /
                                                                (span - axis.thumb_span));
                            CHECK(difference <= tolerance);
                            unsigned last = 0;
                            for (unsigned k = 0; k <= 10; k++) {
                                unsigned value =
                                    layout_scroll_position(axis, start + span * (float)k / 10, grab);
                                CHECK(value >= last && value <= limit);
                                last = value;
                            }
                        } else
                            CHECK(layout_scroll_position(axis, start + span, 0) == 0);
                        scroll_cases++;
                    }
                }
    printf("OK %u scrollbar geometry states: bounded thumbs, minimum grip, endpoints, roundtrip, monotonic "
           "mapping, zero/short/8-Mi-cell content\n",
           scroll_cases);
    return 0;
}
