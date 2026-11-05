"""Messages PyLint ignorés : grammaire bornée, sans import ni exécution du projet."""

import re

MAX_DISABLED_BYTES = 4095
MAX_DISABLED_RULES = 64
MAX_DISABLED_NAME = 95


def normalize_disabled(value):
    """Canonical comma-separated IDs/symbols; no categories, options or hooks."""
    if not isinstance(value, str):
        raise ValueError("La liste des règles PyLint doit être du texte.")
    if len(value.encode("utf-8")) > MAX_DISABLED_BYTES:
        raise ValueError("Liste PyLint limitée à 4 095 octets.")
    rules = []
    for raw in re.split(r"[,; \t\r\n]+", value):
        if not raw:
            continue
        if re.fullmatch(r"[EFWCRIefwcri][0-9]{4}", raw):
            rule = raw.upper()
        elif (
            len(raw) <= MAX_DISABLED_NAME
            and re.fullmatch(r"[a-zA-Z][a-zA-Z0-9]*(?:-[a-zA-Z0-9]+)*", raw)
            and raw.lower() != "all"
            and len(raw) > 1
        ):
            rule = raw.lower()
        else:
            raise ValueError(
                "Règle PyLint invalide : utilisez un ID (W0611) ou un nom (unused-import)."
            )
        if rule not in rules:
            rules.append(rule)
            if len(rules) > MAX_DISABLED_RULES:
                raise ValueError("Limite : 64 règles PyLint distinctes.")
    return ",".join(rules)


def diagnostic_rule(code, symbol):
    """Only concrete PyLint IDs; syntax always remains actionable as an error."""
    if (
        not isinstance(code, str)
        or not isinstance(symbol, str)
        or not re.fullmatch(r"[EFWCRI][0-9]{4}", code)
        or code == "E0001"
        or symbol == "syntax-error"
    ):
        return ""
    return code


def append_disabled(value, code, symbol):
    """Append the displayed rule, not a row index; preserve aliases and limits."""
    code = diagnostic_rule(code, symbol)
    if not code:
        raise ValueError("Ce diagnostic ne peut pas être désactivé ici.")
    current = normalize_disabled(value)
    tokens = current.split(",") if current else []
    if code in tokens or (symbol and symbol in tokens):
        return current
    return normalize_disabled(",".join([*tokens, code]))
