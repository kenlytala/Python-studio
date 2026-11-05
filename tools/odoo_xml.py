"""Static, bounded Odoo19 XML checks. No ORM/database/network/file imports."""

import re
from xml.parsers import expat

XML_RULES = {
    "W9001": (
        "odoo-xml-duplicate-id",
        "Identifiant local réutilisé : %s (peut être une mise à jour volontaire).",
    ),
    "W9002": ("odoo-xml-record-model", "Attribut model absent ou vide sur record."),
    "W9003": ("odoo-xml-empty-id", "Identifiant XML vide."),
    "W9004": (
        "odoo-xml-id-whitespace",
        "Identifiant XML contenant un espace ou caractère de contrôle : %s.",
    ),
    "W9005": (
        "odoo19-xml-tree",
        "Odoo19 : utilisez list au lieu de tree pour une vue de liste.",
    ),
    "W9006": ("odoo-xml-field-name", "Attribut name absent ou vide sur field."),
    "W9007": (
        "odoo19-xml-attrs-states",
        "Odoo19 : attrs/states sont remplacés par les expressions de modificateurs.",
    ),
    "W9008": (
        "odoo19-xml-openerp",
        "Racine openerp ancienne : utilisez odoo pour Odoo19.",
    ),
    "W9009": (
        "odoo-xml-root",
        "Racine XML non standard pour des données/vues Odoo19 : %s.",
    ),
    "W9010": (
        "odoo-xml-external-include",
        "Inclusion XML externe non résolue par cette analyse statique.",
    ),
}
XML_RULE_TOKENS = frozenset(XML_RULES) | frozenset(
    value[0] for value in XML_RULES.values()
)


def is_odoo_diagnostic(code):
    return bool(re.fullmatch(r"[ECWRIF]81[0-9]{2}", code) or code in XML_RULES)


def odoo_xml_analyze(source, filename, enabled, disabled=""):
    raw = source.encode("utf-8")
    backend = "odoo-xml" if enabled else "xml"
    version = "19.0" if enabled else ""
    if len(raw) > 2 * 1024 * 1024:
        return (
            "error",
            version,
            "XML trop volumineux pour cette analyse (2 Mio).",
            [],
            0,
        )
    for token in filter(None, disabled.split(",")):
        if (
            re.fullmatch(r"[ECWRIF]90[0-9]{2}", token)
            or token.startswith(("odoo-xml-", "odoo19-xml-"))
        ) and token not in XML_RULE_TOKENS:
            return "error", version, "Règle XML Odoo inconnue : " + token, [], 0
    # Editors already decoded the original file/BOM to Unicode. The wire is
    # UTF-8 regardless of an original UTF-16/Latin1 declaration; override it
    # without rewriting source or shifting diagnostic byte positions.
    parser = expat.ParserCreate(encoding="utf-8", namespace_separator="}")
    records = []
    total = 0
    stack = []
    ids = {}
    nodes = 0
    document_kind = ""
    ignored = set(disabled.split(",")) if disabled else set()

    def position(index):
        return raw.count(b"\n", 0, index) + 1, index - (raw.rfind(b"\n", 0, index) + 1)

    def emit(code, args=()):
        nonlocal total
        symbol, message = XML_RULES[code]
        if code in ignored or symbol in ignored:
            return
        total += 1
        row, col = position(parser.CurrentByteIndex)
        if len(records) < 256:
            records.append(
                (
                    "W",
                    row,
                    col,
                    row,
                    col,
                    code,
                    symbol,
                    message % args if args else message,
                )
            )

    class LimitError(ValueError):
        pass

    def doctype(*args):
        raise LimitError(
            "Les déclarations DTD/entités ne sont pas autorisées ; aucune ressource externe n’est chargée."
        )

    def start(name, attrs):
        nonlocal nodes, document_kind
        nodes += 1
        if nodes > 20000 or len(stack) >= 256:
            raise LimitError("Limite XML atteinte (20 000 éléments / profondeur256).")
        tag = name.rsplit("}", 1)[-1]
        if not stack:
            document_kind = (
                "data"
                if tag in ("odoo", "openerp")
                else "owl"
                if tag == "templates"
                else "view"
                if tag
                in (
                    "form",
                    "list",
                    "tree",
                    "search",
                    "kanban",
                    "graph",
                    "pivot",
                    "calendar",
                    "activity",
                    "cohort",
                    "gantt",
                )
                else "other"
            )
            if enabled and tag == "openerp":
                emit("W9008")
            if enabled and document_kind == "other":
                emit("W9009", (tag,))
        inherited = stack[-1][1] if stack else document_kind == "view"
        view = inherited or tag == "field" and attrs.get("name") == "arch"
        if enabled:
            value = (
                attrs.get("id")
                if document_kind == "data"
                else attrs.get("t-name")
                if document_kind == "owl"
                else None
            )
            if value is not None:
                if not value:
                    emit("W9003")
                elif any(c.isspace() or ord(c) < 32 or ord(c) == 127 for c in value):
                    emit("W9004", (value,))
                elif value in ids:
                    emit("W9001", (value,))
                else:
                    if len(ids) >= 4096:
                        raise LimitError(
                            "Limite de 4 096 identifiants locaux atteinte."
                        )
                    ids[value] = True
            if document_kind == "data" and tag == "record" and not attrs.get("model"):
                emit("W9002")
            if (
                document_kind in ("data", "view")
                and tag == "field"
                and not attrs.get("name")
            ):
                emit("W9006")
            if view and tag == "tree":
                emit("W9005")
            if view and ("attrs" in attrs or "states" in attrs):
                emit("W9007")
            if tag == "include" and "href" in attrs and "}" in name:
                emit("W9010")
        stack.append((tag, view))

    def end(name):
        stack.pop()

    parser.StartElementHandler = start
    parser.EndElementHandler = end
    parser.StartDoctypeDeclHandler = doctype
    parser.EntityDeclHandler = doctype
    parser.ExternalEntityRefHandler = lambda *args: 0
    parser.SetParamEntityParsing(expat.XML_PARAM_ENTITY_PARSING_NEVER)
    try:
        parser.Parse(raw, True)
    except (expat.ExpatError, LimitError) as error:
        index = getattr(parser, "ErrorByteIndex", -1)
        if index < 0:
            index = parser.CurrentByteIndex
        index = max(0, index)
        row, col = position(index)
        syntax = ("E", row, col, row, col, "E0001", "syntax-error", str(error))
        return (
            backend,
            version,
            "Syntaxe XML invalide ou contrôle incomplet.",
            ([syntax] + records)[:256],
            total + 1,
        )
    status = (
        "Odoo19 XML · contrôles statiques locaux, sans base ni résolution globale des références."
        if enabled
        else "Syntaxe XML · aucune ressource externe chargée."
    )
    return backend, version, status, records, total
