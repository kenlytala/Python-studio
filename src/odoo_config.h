#ifndef PYTHON_STUDIO_ODOO_CONFIG_H
#define PYTHON_STUDIO_ODOO_CONFIG_H
#include "lint_rules.h"
typedef struct {
    bool enabled;
    char version[8];
    LintRules ignored;
} OdooConfig;
void odoo_config_init(OdooConfig *config);
bool odoo_config_valid(const OdooConfig *config);
bool odoo_diagnostic_code(const char *code);
bool odoo_config_file(char *out, size_t cap, const char *settings, const char *project);
bool odoo_config_load(OdooConfig *out, const char *file, const char *project, char *error, size_t cap);
bool odoo_config_save(const OdooConfig *config, const char *file, const char *project, char *error,
                      size_t cap);
#endif
