/* Real frame/modal dispatch, actual bounded worker and per-project Odoo UI. */
static void feature_pause(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec delay = {0, 1000000};
    nanosleep(&delay, NULL);
#endif
}
static void wait_browser(App *a) {
    BrowserLoadJob *job = a->browser_completion.job;
    CHECK(job);
    unsigned count = 5000;
    while (!atomic_load(&job->done) && count--)
        feature_pause();
    CHECK(atomic_load(&job->done));
    keyboard_frame_input(a, 0, 0, 0, false, false, 0);
    CHECK(!a->browser_completion.job);
}
static void browser_odoo_frame_fixture(void) {
    int first = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    lint_init(&a->linter);
    Buffer b;
    CHECK(buf_init(&b, "original code", "main.py"));
    CHECK(buf_init(&a->field, "", "path") && buf_init(&a->search, "", "search") &&
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
    char root[FS_PATH], folder[FS_PATH], file[FS_PATH], settings[FS_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "ftr", 0, root));
    CHECK(!remove(root) && !_mkdir(root));
#else
    strcpy(root, "/tmp/python-studio-modal-features-XXXXXX");
    CHECK(mkdtemp(root));
#endif
    CHECK(snprintf(folder, sizeof(folder), "%s/sale", root) < FS_PATH);
#ifdef _WIN32
    CHECK(!_mkdir(folder));
#else
    CHECK(!mkdir(folder, 0700));
#endif
    CHECK(snprintf(file, sizeof(file), "%s/order.xml", root) < FS_PATH);
    FILE *out = fopen(file, "wb");
    CHECK(out && fputs("<odoo/>", out) >= 0 && !fclose(out));
    snprintf(a->project, sizeof(a->project), "%s", root);
    snprintf(a->folder, sizeof(a->folder), "%s", root);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", root) < FS_PATH);
    snprintf(settings, sizeof(settings), "%s", a->config);
    frame(a, 1280, 720);
    frame_pointer = (Vector2){900, 500};
    frame_keyboard_available = false;
    mouse_pressed = down = released = false;
    a->modal = M_BROWSER;
    a->browser_mode = 0;
    app_set_field(&a->field, root, false);
    unsigned long source_version = b.version;
    size_t source_cursor = b.cursor;
    keyboard_frame_input(a, KEY_SPACE, 0, KEY_LEFT_CONTROL, false, false, 0);
    wait_browser(a);
    CHECK(a->browser_completion.suggesting && a->browser_count == 2 && a->browser[0].directory &&
          !a->browser[1].directory);
    CHECK(!strcmp(a->field.text, root));
    CHECK(!strcmp(a->folder, root));
    keyboard_frame_input(a, KEY_TAB, 0, 0, false, false, 0);
    CHECK(a->modal == M_BROWSER && !a->browser_completion.suggesting && strstr(a->field.text, "/sale/") &&
          !strcmp(b.text, "original code") && b.version == source_version && b.cursor == source_cursor);
    keyboard_frame_input(a, KEY_Z, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(a->field.text, root));
    a->browser_mode = 1;
    keyboard_frame_input(a, KEY_SPACE, 1, KEY_LEFT_CONTROL, false, false, 0);
    wait_browser(a);
    CHECK(a->browser_count == 1 && a->browser[0].directory);
    keyboard_frame_input(a, KEY_SPACE, 2, KEY_LEFT_CONTROL, false, false, 0);
    app_set_field(&a->field, "changed while reading", false);
    wait_browser(a);
    CHECK(!a->browser_completion.suggesting && !strcmp(a->field.text, "changed while reading"));
    close_modal(a);
    CHECK(a->modal == M_NONE);
    /* Native Odoo settings: draft, apply, field history, persistent per project. */
    app_odoo_load(a);
    CHECK(a->linter.odoo.enabled && !strcmp(a->linter.odoo.version, "19.0"));
    app_odoo_open(a);
    app_set_field(&a->field, "W8165; W9001", false);
    keyboard_frame_input(a, KEY_ENTER, 0, 0, false, false, 0);
    CHECK(a->modal == M_SETTINGS && !strcmp(a->linter.odoo.ignored.text, "W8165,W9001") && a->lint_force);
    char profile[FS_PATH], error[512];
    CHECK(odoo_config_file(profile, sizeof(profile), settings, root));
    OdooConfig loaded;
    CHECK(odoo_config_load(&loaded, profile, root, error, sizeof(error)) && loaded.enabled &&
          loaded.ignored.count == 2);
    app_odoo_open(a);
    app_set_field(&a->field, "--init-hook=bad", false);
    keyboard_frame_input(a, KEY_ENTER, 0, 0, false, false, 0);
    CHECK(a->modal == M_ODOO && *a->odoo_error && a->linter.odoo.ignored.count == 2);
    keyboard_frame_input(a, KEY_ESCAPE, 0, 0, false, false, 0);
    CHECK(a->modal == M_SETTINGS);
    app_odoo_open(a);
    a->odoo_draft_enabled = false;
    app_set_field(&a->field, "W8165", false);
    keyboard_frame_input(a, KEY_ENTER, 2, 0, false, false, 0);
    CHECK(!a->linter.odoo.enabled && !strcmp(a->linter.odoo.ignored.text, "W8165"));
    /* XML gets its own actual analysis cache/force request, not Python compile. */
    snprintf(b.name, sizeof(b.name), "view.xml");
    app_lint_rules_invalidate(a);
    CHECK(a->lint_force && a->lint_force->waiting);
    analysis_reset(a);
    app_browser_completion_shutdown(a);
    lint_close(&a->linter);
    buf_free(&b);
    buf_free(&a->field);
    buf_free(&a->search);
    buf_free(&a->input);
    free(a);
    CHECK(!remove(profile));
    CHECK(!remove(file));
#ifdef _WIN32
    CHECK(!_rmdir(folder) && !_rmdir(root));
#else
    CHECK(!rmdir(folder) && !rmdir(root));
#endif
    frame_editor_calls = 0;
    frame_queued_key = frame_queued_char = 0;
    clock_value = 10;
    printf("OK %d REAL new-feature frame checks: browser Ctrl+Space queue/pressed/repeat, worker "
           "filtering/stale rejection, Tab/undo without source mutation, Odoo19 project apply/cancel/limits "
           "and XML force-analysis\n",
           checks - first);
}
