/* Actual production frame/mouse route + actual app_minimize_window body. */
static void minimize_frame_fixture(void) {
    int first = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    Buffer b;
    CHECK(buf_init(&b, "saved\n", "minimize.py") && buf_insert(&b, "unsaved"));
    b.cursor = 3;
    b.anchor = 1;
    CHECK(buf_add_caret(&b, 5, BUF_NO_SELECTION));
    CHECK(buf_init(&a->field, "", "field") && buf_init(&a->search, "", "search") &&
          buf_init(&a->input, "", "input"));
    a->tabs[0] = &b;
    a->tab_count = 1;
    a->running = true;
    a->font_size = 18;
    a->line_h = 20;
    a->char_w = 10;
    a->code_y = 72;
    a->code_h = 500;
    a->code_bottom = 572;
    a->code_x = 240;
    a->code_right = 1240;
    strcpy(a->project, "/minimize-only-no-files");
    char source[64];
    snprintf(source, sizeof(source), "%s", b.text);
    size_t cursor = b.cursor, anchor = b.anchor;
    unsigned long version = b.version;
    int history = b.undo.count, extra = b.extra_count;
    frame(a, 1280, 720);
    frame_pointer = (Vector2){1250, 30};
    mouse_pressed = true;
    down = released = false;
    blocked_hit = true;
    occupied = (Hit){.action = A_MINIMIZE};
    int before = minimize_requests;
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    CHECK(minimize_requests == before + 1 && a->running && a->modal == M_NONE);
    CHECK(!strcmp(b.text, source) && b.cursor == cursor && b.anchor == anchor && b.version == version &&
          b.undo.count == history && b.extra_count == extra && buf_dirty(&b));
    CHECK(!a->pending && !a->ssh_quit_pending && !a->session_committing_quit);
    mouse_pressed = true;
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    CHECK(minimize_requests == before + 2 && a->running);
    a->running = false;
    app_minimize_window(a);
    CHECK(minimize_requests == before + 2);
    app_minimize_window(NULL);
    CHECK(minimize_requests == before + 2);
    mouse_pressed = false;
    blocked_hit = false;
    buf_free(&a->field);
    buf_free(&a->search);
    buf_free(&a->input);
    buf_free(&b);
    free(a);
    frame_editor_calls = 0;
    clock_value = 10;
    printf("OK %d REAL minimize frame checks: native minimize only, repeated click, stopped/null guards, "
           "dirty text/cursor/multicursors/history/session/SSH state intact; no window\n",
           checks - first);
}
