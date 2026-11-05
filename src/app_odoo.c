#include "app_internal.h"
#include <stdio.h>
#include <string.h>
void app_odoo_load(App *a) {
    OdooConfig next;
    odoo_config_init(&next);
    a->odoo_error[0] = 0;
    char file[BUF_PATH];
    if (!odoo_config_file(file, sizeof(file), a->config, a->project))
        snprintf(a->odoo_error, sizeof(a->odoo_error), "Chemin de configuration Odoo invalide.");
    else if (!odoo_config_load(&next, file, a->project, a->odoo_error, sizeof(a->odoo_error)) &&
             *a->odoo_error)
        next.enabled = false;
    lint_set_odoo(&a->linter, &next, NULL, 0);
    if (*a->odoo_error && a->running)
        app_toast(a, a->odoo_error, true);
}
static bool commit(App *a, const OdooConfig *next, char *error, size_t cap) {
    char file[BUF_PATH];
    if (!odoo_config_file(file, sizeof(file), a->config, a->project)) {
        if (cap)
            snprintf(error, cap, "Chemin de configuration Odoo invalide.");
        return false;
    }
    if (!odoo_config_save(next, file, a->project, error, cap))
        return false;
    bool changed =
        a->linter.odoo.enabled != next->enabled || strcmp(a->linter.odoo.ignored.text, next->ignored.text);
    if (!lint_set_odoo(&a->linter, next, error, cap))
        return false;
    if (changed)
        app_lint_rules_invalidate(a);
    return true;
}
void app_odoo_open(App *a) {
    a->modal = M_ODOO;
    a->focus = 0;
    a->odoo_draft_enabled = a->linter.odoo.enabled;
    a->odoo_error[0] = 0;
    app_set_field(&a->field, a->linter.odoo.ignored.text, true);
    app_completion_dismiss(a);
    app_multicursor_reset(a);
}
void app_odoo_apply(App *a) {
    OdooConfig next = a->linter.odoo;
    next.enabled = a->odoo_draft_enabled;
    if (!lint_rules_parse(&next.ignored, a->field.text, a->odoo_error, sizeof(a->odoo_error)))
        return;
    if (!commit(a, &next, a->odoo_error, sizeof(a->odoo_error)))
        return;
    a->modal = M_SETTINGS;
    app_toast(a, "Configuration Odoo19 enregistrée pour ce projet.", false);
}
void app_odoo_ignore(App *a, const char *code, const char *symbol) {
    OdooConfig next = a->linter.odoo;
    char error[512] = {0}, notice[640];
    if (!lint_rules_add(&next.ignored, &a->linter.odoo.ignored, code, symbol, error, sizeof(error))) {
        snprintf(notice, sizeof(notice), "Impossible d’ignorer %s : %s", code, error);
        app_toast(a, notice, true);
        return;
    }
    if (!strcmp(next.ignored.text, a->linter.odoo.ignored.text)) {
        snprintf(notice, sizeof(notice), "Règle %s déjà ignorée dans ce projet Odoo.", code);
        app_toast(a, notice, false);
        return;
    }
    if (!commit(a, &next, error, sizeof(error))) {
        snprintf(notice, sizeof(notice), "Impossible d’ignorer %s : %s", code, error);
        app_toast(a, notice, true);
        return;
    }
    snprintf(notice, sizeof(notice), "Règle %s ignorée dans ce projet Odoo.", code);
    app_toast(a, notice, false);
}
void app_odoo_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    Buffer *b = &a->field;
    size_t incoming = 0, start, end;
    size_t removed = buf_selection(b, &start, &end) ? end - start : 0;
    if (ctrl && !app_key_pressed(a, KEY_A) && !app_key_pressed(a, KEY_C) && !app_key_pressed(a, KEY_X) &&
        app_key_pressed(a, KEY_V)) {
        const char *text = GetClipboardText();
        if (text)
            while (incoming < LINT_RULES_BYTES && text[incoming])
                incoming++;
    } else if (!ctrl) {
        for (int i = 0; i < count; i++) {
            char value[5];
            incoming += (size_t)utf8_encode(chars[i], value);
        }
    }
    if (incoming && b->len - removed + incoming >= LINT_RULES_BYTES) {
        snprintf(a->odoo_error, sizeof(a->odoo_error), "Liste limitée à 4 095 octets.");
        return;
    }
    unsigned long before = b->version;
    app_edit_field(a, b, chars, count, ctrl, shift);
    if (before != b->version)
        a->odoo_error[0] = 0;
    if (app_key_pressed(a, KEY_ENTER) || app_key_pressed(a, KEY_KP_ENTER))
        app_odoo_apply(a);
}
