/* True update/modal/field routing + actual native rules controller/profile. */
static void lint_rules_frame_fixture(void) {
    int first = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    lint_init(&a->linter);
    Buffer b, other;
    CHECK(buf_init(&b, "source", "draft.py") && buf_init(&other, "other", "other.py"));
    CHECK(buf_insert(&b, "unsaved"));
    b.anchor = 1;
    CHECK(buf_add_caret(&b, 3, BUF_NO_SELECTION));
    char original[64];
    snprintf(original, sizeof(original), "%s", b.text);
    size_t cursor = b.cursor, anchor = b.anchor;
    int history = b.undo.count, extra = b.extra_count;
    a->tabs[0] = &b;
    a->tabs[1] = &other;
    a->tab_count = 2;
    a->running = true;
    a->font_size = 18;
    a->line_h = 20;
    a->char_w = 10;
    a->code_y = 72;
    a->code_h = 500;
    a->code_bottom = 572;
    a->code_x = 240;
    a->code_right = 1240;
    CHECK(buf_init(&a->field, "", "champ"));
    CHECK(buf_init(&a->search, "", "recherche"));
    CHECK(buf_init(&a->input, "", "console"));
    char dir[BUF_PATH], file[BUF_PATH];
#ifdef _WIN32
    char tmp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, tmp) && GetTempFileNameA(tmp, "lsf", 0, dir));
    CHECK(!remove(dir) && !_mkdir(dir));
#else
    strcpy(dir, "/tmp/python-studio-lint-frame-XXXXXX");
    CHECK(mkdtemp(dir));
#endif
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", dir) < BUF_PATH);
    snprintf(a->project, sizeof(a->project), "%s", dir);
    frame(a, 1280, 720);
    frame_pointer = (Vector2){900, 500};
    down = released = false;
    frame_keyboard_available = true;
    keyboard_test_layout(frame_keyboard_symbols, 1);
    app_lint_rules_load(a);
    CHECK(!*a->lint_rules_error && !a->linter.disabled.count);
    /* Mouse dispatch reaches the same rules controller as the production action. */
    a->modal = M_SETTINGS;
    occupied = (Hit){.action = A_LINT_RULES};
    blocked_hit = true;
    mouse_pressed = true;
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    blocked_hit = false;
    mouse_pressed = false;
    CHECK(a->modal == M_LINT_RULES && !a->field.len);
    SetClipboardText("w0611; C0114");
    keyboard_frame_input(a, KEY_V, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(a->field.text, "w0611; C0114"));
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!a->field.len && a->tab_count == 2);
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(a->field.text, "w0611; C0114"));
    a->analyses[0] = calloc(1, sizeof(LintDocument));
    a->analyses[1] = calloc(1, sizeof(LintDocument));
    CHECK(a->analyses[0] && a->analyses[1]);
    a->analyses[0]->valid = a->analyses[1]->valid = true;
    a->lint_auto = false;
    keyboard_frame_input(a, KEY_ENTER, 0, 0, false, false, 0);
    CHECK(a->modal == M_SETTINGS && !strcmp(a->linter.disabled.text, "W0611,C0114") &&
          a->linter.disabled.count == 2);
    CHECK(a->lint_force && !a->lint_force->valid && a->lint_force->waiting &&
          lint_ready(a->lint_force, runner_clock()) && !a->analyses[1]);
    CHECK(!strcmp(b.text, original) && b.cursor == cursor && b.anchor == anchor && b.undo.count == history &&
          b.extra_count == extra && a->tab_count == 2);
    CHECK(lint_rules_file(file, sizeof(file), a->config));
    LintRules saved;
    CHECK(lint_rules_load(&saved, file, a->lint_rules_error, sizeof(a->lint_rules_error)) &&
          !strcmp(saved.text, "W0611,C0114"));
    App *restored = calloc(1, sizeof(*restored));
    CHECK(restored);
    lint_init(&restored->linter);
    snprintf(restored->config, sizeof(restored->config), "%s", a->config);
    strcpy(restored->project, "/different-project");
    app_lint_rules_load(restored);
    CHECK(!strcmp(restored->linter.disabled.text, "W0611,C0114"));
    lint_close(&restored->linter);
    free(restored);
    app_lint_rules_open(a);
    app_set_field(&a->field, "--init-hook=bad", false);
    keyboard_frame_input(a, KEY_ENTER, 1, 0, false, false, 0);
    CHECK(a->modal == M_LINT_RULES && *a->lint_rules_error &&
          !strcmp(a->linter.disabled.text, "W0611,C0114"));
    keyboard_frame_input(a, KEY_ESCAPE, 0, 0, false, false, 0);
    CHECK(a->modal == M_SETTINGS && !strcmp(a->linter.disabled.text, "W0611,C0114"));
    /* Oversize paste doesn't change even the field's history. */
    app_lint_rules_open(a);
    app_set_field(&a->field, "W0611", false);
    CHECK(buf_insert(&a->field, ",C0114"));
    int draft_history = a->field.undo.count;
    char huge[4097];
    memset(huge, 'a', 4096);
    huge[4096] = 0;
    SetClipboardText(huge);
    keyboard_frame_input(a, KEY_V, 2, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(a->field.text, "W0611,C0114") && a->field.undo.count == draft_history &&
          *a->lint_rules_error);
    /* Failed persistence cannot commit/clear the last active configuration. */
    char settings[BUF_PATH];
    snprintf(settings, sizeof(settings), "%s", a->config);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/settings.cfg", dir) < BUF_PATH);
    app_set_field(&a->field, "E0602", false);
    LintDocument *before = a->lint_force;
    keyboard_frame_input(a, KEY_ENTER, 2, 0, false, false, 0);
    CHECK(a->modal == M_LINT_RULES && *a->lint_rules_error &&
          !strcmp(a->linter.disabled.text, "W0611,C0114") && a->lint_force == before);
    snprintf(a->config, sizeof(a->config), "%s", settings);
    /* Clear only the draft, then apply via native repeated Enter. */
    occupied = (Hit){.action = A_LINT_RULES_CLEAR};
    blocked_hit = true;
    mouse_pressed = true;
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    blocked_hit = false;
    mouse_pressed = false;
    CHECK(!a->field.len && a->linter.disabled.count == 2);
    keyboard_frame_input(a, KEY_ENTER, 2, 0, false, false, 0);
    CHECK(a->modal == M_SETTINGS && !a->linter.disabled.count);
    CHECK(lint_rules_load(&saved, file, a->lint_rules_error, sizeof(a->lint_rules_error)) && !saved.count);
    CHECK(!strcmp(b.text, original) && b.cursor == cursor && b.anchor == anchor && b.undo.count == history &&
          b.extra_count == extra);
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
    frame_keyboard_available = false;
    frame_editor_calls = 0;
    frame_queued_key = frame_queued_char = 0;
    clock_value = 10;
    printf("OK %d REAL lint-settings frame checks: mouse/modal dispatch, AZERTY field history, immediate "
           "persistent apply, manual refresh, stale-cache invalidation, cancel, limits/failed-save rollback, "
           "clear/re-enable, global reload; no window\n",
           checks - first);
}
