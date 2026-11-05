"""Adapter for the external Python/Pylint process; never executes editor code.

Embedded in the native C binary by CMake. No runtime path to this file is needed.
The protocol contains only ASCII (all text fields are UTF-8 encoded as hex).
"""
import base64
import contextlib
import io
import linecache
import os
import sys
import warnings

_MAX_ITEMS = 256
# The trusted worker/CMake prepends the shared, stdlib-only grammar.
_normalize_disabled = globals()["normalize_disabled"]


class _ToolOutput(io.StringIO):
    """Keep startup errors (including optparse/argparse usage failures) bounded."""

    def write(self, text):
        length = len(text)
        tail = (self.getvalue() + text[-8192:])[-8192:]
        self.seek(0)
        self.truncate(0)
        super().write(tail)
        return length


def _failure_status(error, version, output):
    if isinstance(error, SystemExit):
        title = "PyLint %s : démarrage interrompu (code %s)" % (version, error.code)
    else:
        title = "PyLint %s : analyse impossible (%s)" % (version, type(error).__name__)
    # The useful error follows the potentially very long usage listing. Never
    # show only a numeric SystemExit value or the beginning of that listing.
    lines = [line.strip() for line in output.getvalue().splitlines() if line.strip()]
    detail = lines[-1] if lines else str(error)
    return title + " : " + detail


def _hex(value, limit):
    text = str(value).replace("\n", " ").replace("\r", " ").replace("\t", " ")
    text = "".join(c if ord(c) >= 32 else " " for c in text)
    return text.encode("utf-8", "replace")[:limit].decode("utf-8", "ignore").encode("utf-8").hex()


def _emit(backend, version, status, records, total):
    output = ["PRISME-LINT\t1\t%s\t%s\t%s" % (backend, _hex(version, 48), _hex(status, 384))]
    for severity, line, col, end_line, end_col, code, symbol, message in records:
        output.append("D\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s" % (
            severity, max(1, line), max(0, col), max(1, end_line), max(0, end_col),
            code, _hex(symbol, 95), _hex(message, 700)))
    output.append("END\t%d\t%d" % (len(records), total))
    sys.__stdout__.write("\n".join(output) + "\n")
    sys.__stdout__.flush()


def _prisme_analyze(source, filename, project, import_dirs=(), disabled="", odoo=False, odoo_disabled=""):
    if type(odoo) is not bool:
        _emit("error", "", "Configuration Odoo invalide.", [], 0)
        return
    if filename.lower().endswith((".xml", ".xsl", ".xslt", ".svg")):
        try:
            ignored = _normalize_disabled(odoo_disabled)
            analyze = globals().get("odoo_xml_analyze")
            if analyze is None:
                raise RuntimeError("Analyseur XML absent du paquet.")
            _emit(*analyze(source, filename, odoo, ignored if odoo else ""))
        except (ValueError, RuntimeError, UnicodeError) as error:
            _emit("error", "19.0", "Analyse XML indisponible : " + str(error), [], 0)
        return
    # compile() validates syntax (including 'return' outside a function), but
    # the resulting code object is NEVER executed.
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", SyntaxWarning)
            compile(source, filename, "exec", dont_inherit=True)
    except SyntaxError as error:
        lines = source.splitlines()
        line = max(1, error.lineno or 1)
        end_line = max(line, getattr(error, "end_lineno", None) or line)
        original = lines[line - 1] if line <= len(lines) else ""
        ending = lines[end_line - 1] if end_line <= len(lines) else original
        col = len(original[:max(0, (error.offset or 1) - 1)].encode("utf-8"))
        end_col = len(ending[:max(0, (getattr(error, "end_offset", None) or (error.offset or 1)) - 1)].encode("utf-8"))
        _emit("syntax", "", "", [("E", line, col, end_line, end_col, "E0001", "syntax-error", error.msg)], 1)
        return
    except Exception as error:
        _emit("error", "", "Vérification de syntaxe impossible : " + str(error), [], 0)
        return

    try:
        disabled = _normalize_disabled(disabled)
        scoped = _normalize_disabled(odoo_disabled)
        if odoo:
            xml_tokens = globals().get("XML_RULE_TOKENS", frozenset())
            disabled = ",".join(dict.fromkeys([*filter(None, disabled.split(",")), *(token for token in scoped.split(",") if token and token not in xml_tokens)]))
    except (ValueError, UnicodeError) as error:
        _emit("error", "", "Configuration PyLint invalide : " + str(error), [], 0)
        return

    try:
        # Old PyLint/astroid dependencies may warn while importing. Native C
        # merges stderr/stdout, so even these must not corrupt the protocol.
        with contextlib.redirect_stdout(_ToolOutput()), contextlib.redirect_stderr(_ToolOutput()):
            import pylint
            from pylint.lint import Run
            from pylint.reporters import BaseReporter
    except ImportError:
        _emit("syntax", "", "PyLint non installé dans cet interpréteur isolé. Installez-le avec Python -m pip install pylint.", [], 0)
        return

    # User-selected directories are data, never hooks/commands or PYTHONPATH.
    # Tool imports precede them so a framework's pylint.py cannot replace PyLint.
    if not isinstance(import_dirs, (tuple, list)) or len(import_dirs) > 8:
        _emit("error", pylint.__version__, "Liste de dossiers d’import invalide.", [], 0)
        return
    for path in import_dirs:
        if not isinstance(path, str) or not os.path.isabs(path) or not os.path.isdir(path):
            _emit("error", pylint.__version__, "Dossier d’import introuvable ou inaccessible : " + str(path), [], 0)
            return

    plugin_version = ""
    if odoo:
        try:
            # Import the one approved plugin BEFORE any project/import path is
            # appended, so a project pylint_odoo.py cannot replace it.
            with contextlib.redirect_stdout(_ToolOutput()), contextlib.redirect_stderr(_ToolOutput()):
                import pylint_odoo
                from pylint_odoo import misc as odoo_misc
            if "19.0" not in odoo_misc.DFTL_VALID_ODOO_VERSIONS:
                raise RuntimeError("Cette version de pylint-odoo ne reconnaît pas Odoo19.")
            plugin_version = getattr(pylint_odoo, "__version__", "?")
        except (ImportError, AttributeError, RuntimeError) as error:
            _emit("error", pylint.__version__, "Vérification Odoo19 indisponible : " + str(error) + ". Installez pylint-odoo dans cet interpréteur ou désactivez Odoo pour ce projet.", [], 0)
            return

    buckets = {"E": [], "W": [], "I": []}
    total = 0

    class Reporter(BaseReporter):
        def handle_message(self, message):
            nonlocal total
            total += 1
            severity = "E" if message.category in ("error", "fatal") else "W" if message.category == "warning" else "I"
            if len(buckets[severity]) < _MAX_ITEMS:
                buckets[severity].append((severity, message.line, message.column,
                                         getattr(message, "end_line", None) or message.line,
                                         getattr(message, "end_column", None) if getattr(message, "end_column", None) is not None else message.column,
                                         message.msg_id, message.symbol, message.msg))

        def _display(self, layout):
            pass

    # Isolated Python was started by C (-I -B). Tool imports above cannot be
    # shadowed by a pylint.py/linecache.py file in the project. Local paths are
    # appended only afterwards, so static imports can still be resolved.
    for path in (project, os.path.dirname(filename), *import_dirs):
        if path and path not in sys.path:
            sys.path.append(path)
    linecache.cache[filename] = (len(source), None, source.splitlines(True), filename)
    previous_stdin = sys.stdin
    status = "Odoo19 · pylint-odoo " + plugin_version if odoo else ""
    backend = "pylint"
    tool_output = _ToolOutput()
    try:
        # An explicit null-device config suppresses project/user plugins and
        # init-hook without creating temporary directories that could survive a kill.
        sys.stdin = io.TextIOWrapper(io.BytesIO(source.encode("utf-8")), encoding="utf-8")
        with contextlib.redirect_stdout(tool_output), contextlib.redirect_stderr(tool_output):
            # --source-roots is unknown to older PyLint 2.x (e.g. 2.12/2.13).
            # The project and document directories are already on sys.path;
            # no version-specific CLI option is needed for local imports.
            arguments = ["--rcfile=" + os.devnull, "--from-stdin", "--reports=no", "--score=no",
                         "--persistent=no", "--jobs=1"]
            if odoo:
                arguments += ["--load-plugins=pylint_odoo", "--valid-odoo-versions=19.0"]
            if disabled:
                arguments.append("--disable=" + disabled)
            run = Run(arguments + [filename], reporter=Reporter(), exit=False)
            # Validate against THIS interpreter's catalog, not a GUI-side list
            # that could drift between PyLint versions. Unknown/removed rules
            # must not silently look like a clean analysis.
            if disabled:
                for rule in disabled.split(","):
                    try:
                        run.linter.msgs_store.get_message_definitions(rule)
                    except Exception:
                        backend = "error"
                        status = "PyLint %s : règle ignorée inconnue ou indisponible : %s" % (pylint.__version__, rule)
                        break
    except (Exception, SystemExit) as error:
        backend = "error"
        status = _failure_status(error, getattr(pylint, "__version__", "?"), tool_output)
    finally:
        sys.stdin = previous_stdin
    # Keep real errors even if hundreds of style messages were emitted first.
    records = (buckets["E"] + buckets["W"] + buckets["I"])[:_MAX_ITEMS]
    _emit(backend, getattr(pylint, "__version__", ""), status, records, total)
