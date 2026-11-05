/* Genuine mouse/update action -> quick controller -> persisted rules/cache. */
static LintDocument *quick_publish(App *a) {
    analysis_reset(a);
    LintDocument *doc = analysis_document(a, a->active, true);
    CHECK(doc);
    char file[BUF_PATH];
    CHECK(analysis_filename(a, a->active, file));
    lint_observe(doc, app_buffer(a)->version, file, a->project, runner_clock());
    doc->analyzed_version = app_buffer(a)->version;
    doc->valid = true;
    doc->waiting = false;
    doc->report.backend = LINT_BACKEND_PYLINT;
    doc->report.count = doc->report.total = 2;
    doc->report.revision = ++a->linter.report_sequence;
    strcpy(doc->report.items[0].code, "C0114");
    strcpy(doc->report.items[0].symbol, "missing-module-docstring");
    strcpy(doc->report.items[1].code, "W0611");
    strcpy(doc->report.items[1].symbol, "unused-import");
    for (int i = 0; i < 2; i++) {
        doc->report.items[i].row = 0;
        doc->report.items[i].column = (size_t)i;
        doc->report.items[i].end_column = (size_t)i + 1;
        strcpy(doc->report.items[i].message, "real-looking diagnostic");
    }
    CHECK(app_diagnostics(a) == &doc->report);
    app_lint_quick_bind(a, &doc->report);
    return doc;
}
static void quick_click(App *a, int action) {
    occupied = (Hit){.action = A_LINT_IGNORE, .arg = action};
    blocked_hit = true;
    mouse_pressed = true;
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    blocked_hit = false;
    mouse_pressed = false;
}
static void lint_quick_frame_fixture(void) {
    int first = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    lint_init(&a->linter);
    Buffer b, other;
    CHECK(buf_init(&b, "import os\nprint(inconnu)\n", "quick.py") && buf_init(&other, "other", "other.py"));
    CHECK(buf_insert(&b, "# dirty\n"));
    b.cursor = 3;
    b.anchor = 1;
    CHECK(buf_add_caret(&b, 5, BUF_NO_SELECTION));
    char source[128];
    snprintf(source, sizeof(source), "%s", b.text);
    size_t cursor = b.cursor, anchor = b.anchor;
    int history = b.undo.count, extra = b.extra_count;
    unsigned long version = b.version;
    a->tabs[0] = &b;
    a->tabs[1] = &other;
    a->tab_count = 2;
    a->running = true;
    a->terminal = true;
    a->console_mode = 1;
    a->lint_auto = false;
    a->font_size = 18;
    a->line_h = 20;
    a->char_w = 10;
    a->code_y = 72;
    a->code_h = 400;
    a->code_bottom = 472;
    a->code_x = 240;
    a->code_right = 1240;
    CHECK(buf_init(&a->field, "", "champ") && buf_init(&a->search, "", "recherche") &&
          buf_init(&a->input, "", "console"));
    char dir[BUF_PATH], file[BUF_PATH], error[512];
#ifdef _WIN32
    char tmp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, tmp) && GetTempFileNameA(tmp, "lqf", 0, dir));
    CHECK(!remove(dir) && !_mkdir(dir));
#else
    strcpy(dir, "/tmp/python-studio-lint-quick-XXXXXX");
    CHECK(mkdtemp(dir));
#endif
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", dir) < BUF_PATH);
    snprintf(a->project, sizeof(a->project), "%s", dir);
    frame(a, 1280, 720);
    frame_pointer = (Vector2){1200, 550};
    frame_wheel = 0;
    down = released = mouse_pressed = false;
    frame_keyboard_available = false;
    int warning = lint_rule_action("W0611", "unused-import");
    CHECK(warning >= 0);
    LintDocument *doc = quick_publish(a);
    /* Captured concrete code survives a row reorder; no index can disable C0114. */
    Diagnostic swap = doc->report.items[0];
    doc->report.items[0] = doc->report.items[1];
    doc->report.items[1] = swap;
    quick_click(a, warning);
    CHECK(!strcmp(a->linter.disabled.text, "W0611") && a->console_mode == 1 && a->modal == M_NONE);
    CHECK(a->lint_force && a->lint_force->waiting && !a->lint_force->valid &&
          lint_ready(a->lint_force, runner_clock()));
    CHECK(!a->lint_quick.buffer && !a->lint_quick.report && a->lint_scroll == 0);
    CHECK(!strcmp(source, b.text) && b.cursor == cursor && b.anchor == anchor && b.undo.count == history &&
          b.extra_count == extra && b.version == version);
    CHECK(lint_rules_file(file, sizeof(file), a->config));
    LintRules saved;
    CHECK(lint_rules_load(&saved, file, error, sizeof(error)) && !strcmp(saved.text, "W0611"));
    /* Rapid second click is stale/idempotent. */
    LintDocument *forced = a->lint_force;
    quick_click(a, warning);
    CHECK(!strcmp(a->linter.disabled.text, "W0611") && a->lint_force == forced && a->toast_error);
    CHECK(lint_set_disabled(&a->linter, "", error, sizeof(error)));
    doc = quick_publish(a);
    doc->report.revision++;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count && a->toast_error && doc->valid);
    doc = quick_publish(a);
    a->modal = M_SETTINGS;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    a->modal = M_NONE;
    doc = quick_publish(a);
    a->terminal = false;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    a->terminal = true;
    doc = quick_publish(a);
    a->console_mode = 0;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    a->console_mode = 1;
    doc = quick_publish(a);
    a->active_pane = 1;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    a->active_pane = 0;
    doc = quick_publish(a);
    a->active = 1;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    a->active = 0;
    doc = quick_publish(a);
    strcpy(b.path, "/renamed.py");
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    *b.path = 0;
    doc = quick_publish(a);
    CHECK(buf_insert(&b, "x"));
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    CHECK(buf_undo(&b));
    doc = quick_publish(a);
    doc->report.backend = LINT_BACKEND_SYNTAX;
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count);
    doc = quick_publish(a);
    strcpy(doc->report.items[1].code, "E0001");
    strcpy(doc->report.items[1].symbol, "syntax-error");
    quick_click(a, 1);
    CHECK(!a->linter.disabled.count);
    /* Failed write must keep cached report and configuration intact. */
    doc = quick_publish(a);
    char settings[BUF_PATH];
    snprintf(settings, sizeof(settings), "%s", a->config);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/settings.cfg", dir) < BUF_PATH);
    quick_click(a, warning);
    CHECK(!a->linter.disabled.count && a->toast_error && doc->valid && app_diagnostics(a) == &doc->report &&
          !a->lint_force);
    snprintf(a->config, sizeof(a->config), "%s", settings);
    char many[512] = "";
    for (int i = 0; i < 64; i++) {
        char token[16];
        snprintf(token, sizeof(token), "%sW%04d", i ? "," : "", i);
        strcat(many, token);
    }
    CHECK(lint_set_disabled(&a->linter, many, error, sizeof(error)));
    doc = quick_publish(a);
    quick_click(a, warning);
    CHECK(a->linter.disabled.count == 64 && !strcmp(a->linter.disabled.text, many) && doc->valid);
    CHECK(lint_set_disabled(&a->linter, "unused-import", error, sizeof(error)));
    doc = quick_publish(a);
    quick_click(a, warning);
    CHECK(!strcmp(a->linter.disabled.text, "unused-import") && doc->valid && !a->lint_force);
    /* Closing/releasing the originating document invalidates captured bindings. */
    analysis_release(a, 0);
    CHECK(!a->lint_quick.buffer && !a->lint_quick.report);
    CHECK(!remove(file));
#ifdef _WIN32
    CHECK(!_rmdir(dir));
#else
    CHECK(!rmdir(dir));
#endif
    analysis_reset(a);
    lint_close(&a->linter);
    buf_free(&a->field);
    buf_free(&a->search);
    buf_free(&a->input);
    buf_free(&b);
    buf_free(&other);
    free(a);
    frame_editor_calls = 0;
    frame_queued_key = frame_queued_char = 0;
    clock_value = 10;
    printf("OK %d REAL quick-ignore frame checks: mouse dispatch, concrete displayed code not row index, "
           "persistent append/manual refresh, readonly source/carets/history, report "
           "revisions/tab/pane/modal/path/text guards, syntax blocked, rapid click, failed "
           "save/limits/aliases; no window\n",
           checks - first);
}
