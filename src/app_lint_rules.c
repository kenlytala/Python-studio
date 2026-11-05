#include "app_internal.h"
#include <stdio.h>
#include <string.h>
void app_lint_rules_load(App *a) {
    char file[BUF_PATH];
    LintRules rules;
    a->lint_rules_error[0] = 0;
    if (!lint_rules_file(file, sizeof(file), a->config)) {
        snprintf(a->lint_rules_error, sizeof(a->lint_rules_error), "Chemin des règles PyLint invalide.");
        return;
    }
    if (lint_rules_load(&rules, file, a->lint_rules_error, sizeof(a->lint_rules_error)))
        lint_set_disabled(&a->linter, rules.text, a->lint_rules_error, sizeof(a->lint_rules_error));
}
void app_lint_rules_open(App *a) {
    app_set_field(&a->field, a->linter.disabled.text, true);
    a->modal = M_LINT_RULES;
    a->focus = 0;
    a->lint_rules_error[0] = 0;
    app_completion_dismiss(a);
    app_multicursor_reset(a);
}
static bool commit_rules(App *a, const LintRules *next, char *error, size_t cap) {
    char file[BUF_PATH];
    if (!lint_rules_file(file, sizeof(file), a->config)) {
        if (cap)
            snprintf(error, cap, "Chemin des règles PyLint invalide.");
        return false;
    }
    if (!lint_rules_save(next, file, error, cap))
        return false;
    bool changed = strcmp(next->text, a->linter.disabled.text) != 0;
    if (!lint_set_disabled(&a->linter, next->text, error, cap))
        return false;
    if (changed)
        app_lint_rules_invalidate(a);
    return true;
}
void app_lint_rules_apply(App *a) {
    LintRules next;
    if (!lint_rules_parse(&next, a->field.text, a->lint_rules_error, sizeof(a->lint_rules_error)))
        return;
    if (!commit_rules(a, &next, a->lint_rules_error, sizeof(a->lint_rules_error)))
        return;
    a->modal = M_SETTINGS;
    app_toast(a, next.count ? "Règles PyLint enregistrées." : "Aucune règle PyLint ignorée.", false);
}
void app_lint_quick_ignore(App *a, int action) {
    if (!a || a->active < 0 || a->active >= a->tab_count)
        return;
    Buffer *b = app_buffer(a);
    LintReport *report = app_diagnostics(a);
    char code[6];
    if (a->modal != M_NONE || !a->running || a->ssh_quit_pending || !a->terminal || a->console_mode != 1 ||
        !b || b != a->lint_quick.buffer || b->version != a->lint_quick.version ||
        a->active_pane != a->lint_quick.pane ||
        lint_rules_scope(a->project, b->path, b->name) != a->lint_quick.scope || !report ||
        report != a->lint_quick.report || report->revision != a->lint_quick.revision ||
        (report->backend != LINT_BACKEND_PYLINT && report->backend != LINT_BACKEND_ODOO_XML) ||
        !lint_rule_decode(action, code)) {
        app_toast(a, "Diagnostic périmé : relancez l’analyse avec F4.", true);
        return;
    }
    const Diagnostic *item = NULL;
    for (int i = 0; i < report->count && i < LINT_MAX_ITEMS; i++)
        if (lint_rule_action(report->items[i].code, report->items[i].symbol) == action) {
            item = &report->items[i];
            break;
        }
    if (!item) {
        app_toast(a, "Ce diagnostic ne peut pas être désactivé ici.", true);
        return;
    }
    char symbol[96], error[512] = {0}, notice[640];
    snprintf(symbol, sizeof(symbol), "%s", item->symbol);
    if (odoo_diagnostic_code(code)) {
        app_odoo_ignore(a, code, symbol);
        return;
    }
    LintRules next;
    if (!lint_rules_add(&next, &a->linter.disabled, code, symbol, error, sizeof(error))) {
        snprintf(notice, sizeof(notice), "Impossible d’ignorer %s : %s", code, error);
        app_toast(a, notice, true);
        return;
    }
    if (!strcmp(next.text, a->linter.disabled.text)) {
        snprintf(notice, sizeof(notice), "Règle %s déjà ignorée.", code);
        app_toast(a, notice, false);
        return;
    }
    if (!commit_rules(a, &next, error, sizeof(error))) {
        snprintf(notice, sizeof(notice), "Impossible d’ignorer %s : %s", code, error);
        app_toast(a, notice, true);
        return;
    }
    snprintf(notice, sizeof(notice), "Règle %s ignorée pour tous les projets.", code);
    app_toast(a, notice, false);
}
void app_lint_rules_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
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
            char text[5];
            incoming += (size_t)utf8_encode(chars[i], text);
        }
    }
    if (incoming && b->len - removed + incoming >= LINT_RULES_BYTES) {
        snprintf(a->lint_rules_error, sizeof(a->lint_rules_error), "Liste PyLint limitée à 4 095 octets.");
        return;
    }
    unsigned long before = b->version;
    app_edit_field(a, b, chars, count, ctrl, shift);
    if (b->version != before)
        a->lint_rules_error[0] = 0;
    if (app_key_pressed(a, KEY_ENTER) || app_key_pressed(a, KEY_KP_ENTER))
        app_lint_rules_apply(a);
}
