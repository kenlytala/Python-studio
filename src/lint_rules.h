#ifndef PRISME_LINT_RULES_H
#define PRISME_LINT_RULES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LINT_RULES_BYTES 4096
#define LINT_RULES_MAX 64
#define LINT_RULE_NAME_MAX 95
typedef struct {
    char text[LINT_RULES_BYTES];
    unsigned count;
} LintRules;
/* Atomic parse, ASCII message IDs or symbols, no options/categories/hooks. */
bool lint_rules_parse(LintRules *out, const char *value, char *error, size_t cap);
bool lint_rules_valid(const LintRules *rules);
/* Global profile beside the native settings file, never in a project. */
bool lint_rules_file(char *out, size_t cap, const char *settings);
/* Missing: false + empty error. Failed/invalid load leaves out unchanged. */
bool lint_rules_load(LintRules *out, const char *file, char *error, size_t cap);
bool lint_rules_save(const LintRules *rules, const char *file, char *error, size_t cap);
/* Concrete displayed ID packed into the UI action, never a mutable row index. */
int lint_rule_action(const char *code, const char *symbol);
bool lint_rule_decode(int action, char out[6]);
bool lint_rules_add(LintRules *out, const LintRules *current, const char *code, const char *symbol,
                    char *error, size_t cap);
uint64_t lint_rules_scope(const char *project, const char *path, const char *name);
#endif
