/* The generated include contains the REAL mouse_update, modal_keys and update.
 * Only unrelated services and the OS are substituted; the editor handler is real.
 * A regression to the 1.14 outer F11 dispatcher MUST fail this test. */
static int frame_queued_key, frame_queued_char, frame_editor_calls;
static unsigned frame_keyboard_symbols[KEYBOARD_KEYS];
static bool frame_keyboard_available;
static int frame_keyboard_probes;
static Vector2 frame_pointer;
static float frame_wheel;
int GetKeyPressed(void) {
    int key = frame_queued_key;
    frame_queued_key = 0;
    return key;
}
int GetCharPressed(void) {
    int value = frame_queued_char;
    frame_queued_char = 0;
    return value;
}
Vector2 GetMousePosition(void) {
    return frame_pointer;
}
float GetMouseWheelMove(void) {
    return frame_wheel;
}
bool WindowShouldClose(void) {
    return false;
}
void SetWindowTitle(const char *title) {
    (void)title;
}
static bool frame_keyboard_read(unsigned symbols[KEYBOARD_KEYS], void *context) {
    (void)context;
    frame_keyboard_probes++;
    if (!frame_keyboard_available)
        return false;
    memcpy(symbols, frame_keyboard_symbols, sizeof(frame_keyboard_symbols));
    return true;
}
static void frame_noop(App *a) {
    (void)a;
}
static void frame_integer(App *a, int value) {
    (void)a;
    (void)value;
}
static void frame_save(App *a, bool value) {
    (void)a;
    (void)value;
}
static void frame_analysis(App *a, double now) {
    (void)a;
    (void)now;
}
static bool frame_checkpoint(App *a, bool force) {
    (void)a;
    (void)force;
    return true;
}
static bool frame_snippet_mouse(App *a, float wheel, bool shift) {
    (void)a;
    (void)wheel;
    (void)shift;
    return false;
}
static void frame_ignore_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    (void)a;
    (void)chars;
    (void)count;
    (void)ctrl;
    (void)shift;
}
static void frame_editor_real(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
static void frame_editor_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    frame_editor_calls++;
    frame_editor_real(a, chars, count, ctrl, shift);
}
static void frame_console(App *a, const char *text, Tone tone) {
    (void)a;
    (void)text;
    (void)tone;
}
static void frame_select_match(App *a, int direction, bool first) {
    (void)a;
    (void)direction;
    (void)first;
}
static size_t frame_mouse_position(App *a, Vector2 m) {
    (void)a;
    (void)m;
    return 0;
}
static void frame_context(App *a, Vector2 m) {
    (void)a;
    (void)m;
}
#define action(a, id, arg) app_action(a, id, arg)
#define app_ssh_poll frame_noop
#define app_xml_poll frame_noop
#define poll_files frame_noop
#define poll_runner frame_noop
#define run_code frame_noop
#define refresh_tree frame_noop
#define app_prepare_syntax frame_integer
#define app_update_matches frame_noop
#define app_session_checkpoint frame_checkpoint
#define analysis_update frame_analysis
#define app_completion_poll frame_noop
#define app_snippet_mouse frame_snippet_mouse
#define app_snippet_keys frame_ignore_keys
#define app_filter_palette frame_noop
#define context_open frame_context
#define font_change frame_integer
#define mouse_position frame_mouse_position
#define browser_submit frame_noop
#define context_create frame_noop
#define goto_submit frame_noop
#define save frame_save
#define editor_keys frame_editor_keys
#define invalidate frame_noop
#define emit frame_noop
#define app_completion_show frame_save
#define console_add frame_console
#define select_match frame_select_match
#define keyboard_native_read frame_keyboard_read
#include "prisme_frame_functions.inc"
#undef keyboard_native_read
#undef action
#undef app_ssh_poll
#undef app_xml_poll
#undef poll_files
#undef poll_runner
#undef run_code
#undef refresh_tree
#undef app_prepare_syntax
#undef app_update_matches
#undef app_session_checkpoint
#undef analysis_update
#undef app_completion_poll
#undef app_snippet_mouse
#undef app_snippet_keys
#undef app_filter_palette
#undef context_open
#undef font_change
#undef mouse_position
#undef browser_submit
#undef context_create
#undef goto_submit
#undef save
#undef editor_keys
#undef invalidate
#undef emit
#undef app_completion_show
#undef console_add
#undef select_match

static void search_frame_key(App *a, int key, int channel) {
    keyboard_clear(a);
    if (!channel)
        frame_queued_key = key;
    else if (channel == 1)
        key_press[key] = true;
    else if (channel == 2)
        key_repeat[key] = true;
    else {
        key_down[KEY_LEFT_ALT] = true;
        frame_queued_key = KEY_ENTER;
    }
    update(a, .016f);
    frame_queued_key = frame_queued_char = 0;
    keyboard_clear(a);
}
static void search_frame_pointer(App *a, float x, float y, bool press, bool hold, bool release) {
    frame_pointer = (Vector2){x, y};
    mouse_pressed = press;
    down = hold;
    released = release;
    search_frame_key(a, 0, 0);
    mouse_pressed = released = false;
}
static void search_frame_geometry(App *a) {
    FolderSearchState *s = &a->folder_search;
    s->rows = 8;
    s->columns = 25;
    s->content_rect = (Rectangle){400, 100, 440, 520};
    s->list_rect = (Rectangle){100, 100, 280, 520};
    s->list_rows = 8;
    s->htrack = (Rectangle){466, 600, 350, 12};
    s->vtrack = (Rectangle){824, 138, 12, 450};
    s->horizontal = layout_scroll_axis(s->htrack.x, s->htrack.width, s->columns, s->extent_columns, s->x);
    s->vertical =
        layout_scroll_axis(s->vtrack.y, s->vtrack.height, s->rows, (unsigned)s->view.line_count, s->y);
    if (s->x > s->horizontal.limit)
        s->x = s->horizontal.limit;
    if (s->y > s->vertical.limit)
        s->y = s->vertical.limit;
    s->hthumb = (Rectangle){s->horizontal.thumb_start, s->htrack.y, s->horizontal.thumb_span, 12};
    s->vthumb = (Rectangle){s->vtrack.x, s->vertical.thumb_start, 12, s->vertical.thumb_span};
}
static SearchReport *search_frame_report(void) {
    SearchReport *r = calloc(1, sizeof(*r));
    CHECK(r);
    r->files = calloc(2, sizeof(*r->files));
    CHECK(r->files);
    r->count = 2;
    r->complete = true;
    r->options = folder_search_options();
    strset(r->root, sizeof(r->root), "/frame");
    strset(r->query, sizeof(r->query), "needle");
    char *text = calloc(220000, 1);
    CHECK(text);
    size_t p = 0;
    memcpy(text + p, "needle\n", 7);
    p += 7;
    for (int i = 0; i < 80; i++) {
        memcpy(text + p, "row\n", 4);
        p += 4;
    }
    memcpy(text + p, "α\t", 3);
    p += 3;
    memset(text + p, 'a', 200000);
    p += 200000;
    strcpy(text + p, "\t猫😀 tailneedle\n");
    r->files[0].text = text;
    r->files[1].text = malloc(8);
    CHECK(r->files[1].text);
    strcpy(r->files[1].text, "needle\n");
    char error[256];
    for (int i = 0; i < 2; i++) {
        SearchFile *f = &r->files[i];
        f->len = strlen(f->text);
        snprintf(f->path, sizeof(f->path), "/frame/%d.txt", i);
        snprintf(f->relative, sizeof(f->relative), "%d.txt", i);
        CHECK(folder_search_find(f->text, f->len, "needle", SEARCH_HITS_MAX, &f->hits, &f->count, error,
                                 sizeof(error)));
        r->hit_count += f->count;
    }
    CHECK(r->hit_count == 3);
    return r;
}
static void search_frame_controller(void) {
    int before = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    Buffer b = {0};
    CHECK(buf_init(&b, "source", "frame.py") && buf_insert(&b, "unsaved"));
    b.anchor = 2;
    b.cursor = 5;
    a->tabs[0] = &b;
    a->tab_count = 1;
    a->running = true;
    a->w = width = 1440;
    a->h = height = 900;
    strset(a->project, sizeof(a->project), "/frame");
    FolderSearchState *s = &a->folder_search;
    s->initialized = true;
    s->options = folder_search_options();
    strset(s->project, sizeof(s->project), a->project);
    CHECK(buf_init(&s->folder, "/frame", "dossier") && buf_init(&s->query, "needle", "texte"));
    for (int i = 0; i < 4; i++)
        CHECK(buf_init(&s->filters[i], "", "filtre"));
    s->report = search_frame_report();
    s->file = s->hit = -1;
    app_folder_search_select(a, 0, 0);
    search_frame_geometry(a);
    CHECK(s->extent_columns == 200022 && s->view.line_count == 83 && s->horizontal.limit == 199997);
    unsigned long version = b.version;
    size_t cursor = b.cursor, anchor = b.anchor;
    int undo = b.undo.count;
    unsigned long search_version = s->view.version, query_version = s->query.version;
    char *source = s->report->files[0].text;
    a->modal = M_FOLDER_SEARCH;
    blocked_hit = mouse_pressed = down = released = false;
    frame_pointer = (Vector2){600, 400};
    frame_wheel = 0;
    frame_editor_calls = 0;
    /* All initial native window states; actual GetKeyPressed queue, pressed,
       repeat and Alt+Enter alias; all 12 search focus targets. */
    for (int full = 0; full < 2; full++)
        for (int max = 0; max < 2; max++)
            for (int channel = 0; channel < 4; channel++)
                for (int focus = 0; focus < 12; focus++) {
                    fullscreen = full != 0;
                    maximized = max != 0;
                    position = (Vector2){58, 67};
                    int mutations = window_mutations, global_actions = fullscreen_actions;
                    s->maximized = false;
                    s->focus = focus;
                    search_frame_key(a, KEY_F11, channel);
                    CHECK(s->maximized && a->modal == M_FOLDER_SEARCH);
                    CHECK(window_mutations == mutations && fullscreen_actions == global_actions &&
                          fullscreen == (full != 0) && maximized == (max != 0) && position.x == 58 &&
                          position.y == 67 && width == 1440 && height == 900 && !s->job);
                    search_frame_key(a, KEY_F11, channel);
                    CHECK(!s->maximized && window_mutations == mutations &&
                          fullscreen_actions == global_actions);
                    CHECK(s->query.version == query_version && b.version == version && !frame_editor_calls);
                }
    /* No key, no repeated toggle; click suppression cannot swallow F11. */
    s->maximized = false;
    mouse_pressed = down = true;
    frame_pointer = (Vector2){600, 400};
    int mutations = window_mutations;
    search_frame_key(a, KEY_F11, 0);
    CHECK(s->maximized && window_mutations == mutations);
    mouse_pressed = down = false;
    search_frame_key(a, 0, 0);
    CHECK(s->maximized);
    search_frame_key(a, KEY_F11, 1);
    CHECK(!s->maximized);
    /* Horizontal thumb grab, out-of-bounds drag, final move+release in one frame. */
    s->focus = 3;
    s->x = s->y = 0;
    search_frame_geometry(a);
    search_frame_pointer(a, s->hthumb.x + 4, 606, true, true, false);
    CHECK(s->scroll_drag == 1 && s->focus == 3 && s->x == 0);
    search_frame_pointer(a, 100000, 606, false, true, false);
    CHECK(s->x == s->horizontal.limit);
    search_frame_pointer(a, -1000, 606, false, false, true);
    CHECK(!s->x && !s->scroll_drag);
    search_frame_geometry(a);
    search_frame_pointer(a, 815, 606, true, true, false);
    CHECK(s->x == s->horizontal.limit && s->scroll_drag == 1);
    /* Lost capture without a release cancels safely, does not start root dragging. */
    search_frame_pointer(a, 500, 606, false, false, false);
    CHECK(!s->scroll_drag && s->x == s->horizontal.limit);
    search_frame_pointer(a, 815, 606, true, false, true);
    CHECK(!s->scroll_drag && s->x == s->horizontal.limit);
    /* Wheel and arrows stop at the real whole-snapshot extent, not 8 MiB. */
    frame_pointer = (Vector2){600, 400};
    frame_wheel = -1000000;
    key_down[KEY_LEFT_SHIFT] = true;
    update(a, .016f);
    CHECK(s->x == s->horizontal.limit);
    frame_wheel = 1000000;
    update(a, .016f);
    CHECK(!s->x);
    frame_wheel = -1e30f;
    update(a, .016f);
    CHECK(s->x == s->horizontal.limit);
    frame_wheel = 1e30f;
    update(a, .016f);
    CHECK(!s->x);
    frame_wheel = NAN;
    update(a, .016f);
    CHECK(!s->x);
    s->x = 100;
    frame_wheel = .1f;
    update(a, .016f);
    CHECK(s->x == 100);
    frame_wheel = 0;
    keyboard_clear(a);
    s->x = s->horizontal.limit - 2;
    search_frame_key(a, KEY_RIGHT, 1);
    CHECK(s->x == s->horizontal.limit);
    s->x = 2;
    search_frame_key(a, KEY_LEFT, 1);
    CHECK(!s->x);
    /* Vertical bar is also operable, without stealing a dirty editor selection. */
    search_frame_geometry(a);
    search_frame_pointer(a, 830, s->vthumb.y + 3, true, true, false);
    CHECK(s->scroll_drag == 2);
    search_frame_pointer(a, 830, 100000, false, false, true);
    CHECK(s->y == s->vertical.limit && !s->scroll_drag);
    search_frame_geometry(a);
    search_frame_pointer(a, 830, s->vthumb.y + 2, true, true, false);
    search_frame_pointer(a, 830, -1000, false, false, true);
    CHECK(!s->y && !s->scroll_drag);
    /* F11 interrupts a held drag; it never invokes native fullscreen. */
    search_frame_geometry(a);
    search_frame_pointer(a, s->hthumb.x + 2, 606, true, true, false);
    mutations = window_mutations;
    search_frame_key(a, KEY_F11, 0);
    CHECK(s->maximized && !s->scroll_drag && window_mutations == mutations);
    down = false;
    search_frame_key(a, KEY_F11, 0);
    CHECK(!s->maximized);
    /* F3 is NOT swallowed while a bar is held. It reveals the distant match,
       then switches file and resets the drag/extent to the short snapshot. */
    search_frame_geometry(a);
    search_frame_pointer(a, s->hthumb.x + 2, 606, true, true, false);
    search_frame_key(a, KEY_F3, 0);
    search_frame_geometry(a);
    CHECK(s->file == 0 && s->hit == 1 && s->x > 199900 && s->y > 70 && !s->scroll_drag);
    search_frame_key(a, KEY_F3, 0);
    search_frame_geometry(a);
    CHECK(s->file == 1 && s->hit == 0 && s->extent_columns == 7 && !s->x && !s->scroll_drag &&
          !s->horizontal.limit);
    search_frame_pointer(a, 815, 606, true, true, false);
    CHECK(!s->scroll_drag && !s->x);
    down = false;
    search_frame_key(a, KEY_F3, 0);
    search_frame_geometry(a);
    CHECK(s->file == 0 && s->hit == 0 && s->extent_columns == 200022);
    /* Actual text queue, paste, undo, backspace remain read-only in the content. */
    s->focus = 3;
    frame_queued_char = 'X';
    search_frame_key(a, 0, 0);
    search_frame_key(a, KEY_BACKSPACE, 1);
    keyboard_clear(a);
    key_down[KEY_LEFT_CONTROL] = true;
    key_press[KEY_V] = true;
    update(a, .016f);
    keyboard_clear(a);
    key_down[KEY_LEFT_CONTROL] = true;
    key_press[KEY_Z] = true;
    update(a, .016f);
    keyboard_clear(a);
    CHECK(b.version == version && b.cursor == cursor && b.anchor == anchor && b.undo.count == undo &&
          buf_dirty(&b));
    CHECK(!strcmp(b.text, "unsavedsource") && !strcmp(source, s->view.text) &&
          s->view.len == s->report->files[0].len);
    CHECK(s->view.version == search_version && !s->view.undo.count && s->report->hit_count == 3);
    /* Escape also closes a held bar through the REAL modal dispatcher. */
    search_frame_geometry(a);
    search_frame_pointer(a, s->hthumb.x + 2, 606, true, true, false);
    search_frame_key(a, KEY_ESCAPE, 0);
    CHECK(a->modal == M_NONE && !s->scroll_drag);
    down = false;
    mouse_pressed = false;
    /* Outside search, the same actual frame keeps the legacy editor fullscreen. */
    for (int full = 0; full < 2; full++)
        for (int max = 0; max < 2; max++)
            for (int channel = 0; channel < 4; channel++) {
                fullscreen = full != 0;
                maximized = max != 0;
                int globals = fullscreen_actions;
                mutations = window_mutations;
                search_frame_key(a, KEY_F11, channel);
                CHECK(fullscreen != (full != 0) && maximized == (max != 0) &&
                      fullscreen_actions == globals + 1 && window_mutations == mutations + 1);
                search_frame_key(a, KEY_F11, channel);
                CHECK(fullscreen == (full != 0) && fullscreen_actions == globals + 2);
            }
    CHECK(!frame_editor_calls);
    search_frame_key(a, 0, 0);
    CHECK(frame_editor_calls == 1);
    CHECK(b.version == version && b.cursor == cursor && b.anchor == anchor && b.undo.count == undo);
    app_folder_search_cancel(a);
    folder_search_report_free(s->report);
    buf_free(&s->view);
    buf_free(&s->folder);
    buf_free(&s->query);
    for (int i = 0; i < 4; i++)
        buf_free(&s->filters[i]);
    buf_free(&b);
    free(a);
    fullscreen = maximized = mouse_pressed = down = released = blocked_hit = false;
    frame_wheel = 0;
    frame_queued_key = frame_queued_char = 0;
    printf("OK %d REAL native frame checks: queued/pressed/repeat F11 and Alt+Enter x four OS states x 12 "
           "focus targets; actual mouse/modal dispatch, draggable bars, limits, final release, F3, Escape, "
           "readonly dirty source, outside-search fullscreen; no window\n",
           checks - before);
}
