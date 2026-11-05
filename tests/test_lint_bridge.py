"""Headless adapter regressions; real PyLint versions are tested separately in C."""
import io
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

_BRIDGE = Path(sys.argv.pop(1)).resolve()
_namespace = {"__name__": "prisme_lint_bridge_test"}
_RULES = _BRIDGE.with_name("lint_rules.py")
exec(compile(_RULES.read_text(encoding="utf-8"), str(_RULES), "exec"), _namespace)
exec(compile(_BRIDGE.read_text(encoding="utf-8"), str(_BRIDGE), "exec"), _namespace)


class BridgeFailures(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="prisme-bridge-")
        self.addCleanup(self.directory.cleanup)
        self.project = self.directory.name
        self.filename = os.path.join(self.project, "unsaved.py")
        Path(self.filename).write_text("print('on disk')\n", encoding="utf-8")

    def analyze(self, run, source="print(inconnu)\n", import_dirs=()):
        package = types.ModuleType("pylint")
        package.__version__ = "2.12.2"
        lint = types.ModuleType("pylint.lint")
        lint.Run = run
        reporters = types.ModuleType("pylint.reporters")
        reporters.BaseReporter = type("BaseReporter", (), {})
        modules = {"pylint": package, "pylint.lint": lint, "pylint.reporters": reporters}
        captured = io.StringIO()
        previous_stdin = sys.stdin
        with patch.dict(sys.modules, modules), patch.object(sys, "__stdout__", captured), \
                patch.object(sys, "path", sys.path[:]):
            _namespace["_prisme_analyze"](source, self.filename, self.project, import_dirs)
        self.assertIs(sys.stdin, previous_stdin)
        self.assertEqual(Path(self.filename).read_text(encoding="utf-8"), "print('on disk')\n")
        lines = captured.getvalue().splitlines()
        self.assertTrue(lines[0].startswith("PRISME-LINT\t1\t"))
        header = lines[0].split("\t")
        self.assertEqual(len(header), 5)
        self.assertTrue(lines[-1].startswith("END\t"))
        status = bytes.fromhex(header[4]).decode("utf-8")
        self.assertLessEqual(len(status.encode("utf-8")), 384)
        return header[2], status, lines

    def test_compatible_arguments_and_unsaved_stdin(self):
        def run(args, reporter, exit):
            self.assertFalse(exit)
            self.assertEqual(args, ["--rcfile=" + os.devnull, "--from-stdin", "--reports=no",
                                    "--score=no", "--persistent=no", "--jobs=1", self.filename])
            self.assertFalse(any("source-roots" in item for item in args))
            self.assertIn(self.project, sys.path)
            self.assertEqual(sys.stdin.read(), "print(inconnu)\n")
            reporter.handle_message(types.SimpleNamespace(
                category="error", line=1, column=6, end_line=1, end_column=13,
                msg_id="E0602", symbol="undefined-variable", msg="Undefined variable 'inconnu'"))
        backend, status, lines = self.analyze(run)
        self.assertEqual(backend, "pylint")
        self.assertEqual(status, "")
        self.assertEqual(lines[-1], "END\t1\t1")
        self.assertIn("E0602", lines[1])

    def test_systemexit32_retains_actual_stderr_cause(self):
        def run(*args, **kwargs):
            print("usage: pylint [options]", file=sys.stderr)
            print("  option help\n" * 12000, file=sys.stderr)
            print("pylint: error: Unrecognized option found: incompatible-option", file=sys.stderr)
            raise SystemExit(32)
        backend, status, lines = self.analyze(run)
        self.assertEqual(backend, "error")
        self.assertIn("2.12.2", status)
        self.assertIn("code 32", status)
        self.assertIn("Unrecognized option found: incompatible-option", status)
        self.assertNotIn("option help", status)
        self.assertEqual(lines[-1], "END\t0\t0")

    def test_optparse_exit2_retains_stdout_cause(self):
        def run(*args, **kwargs):
            print("pylint: error: no such option: --old-option")
            raise SystemExit(2)
        backend, status, _ = self.analyze(run)
        self.assertEqual(backend, "error")
        self.assertIn("code 2", status)
        self.assertIn("no such option: --old-option", status)

    def test_exception_without_tool_output_remains_informative(self):
        def run(*args, **kwargs):
            raise RuntimeError("Dépendance incompatible avec Python")
        backend, status, _ = self.analyze(run)
        self.assertEqual(backend, "error")
        self.assertIn("RuntimeError", status)
        self.assertIn("Dépendance incompatible avec Python", status)

    def test_non_ascii_tool_error_survives_protocol(self):
        def run(*args, **kwargs):
            print("Impossible de lire le fichier /tmp/été/config.ini", file=sys.stderr)
            raise SystemExit(32)
        backend, status, _ = self.analyze(run)
        self.assertEqual(backend, "error")
        self.assertIn("/tmp/été/config.ini", status)

    def test_numeric_exit_without_output_is_not_a_clean_report(self):
        def run(*args, **kwargs):
            raise SystemExit(32)
        backend, status, lines = self.analyze(run)
        self.assertEqual(backend, "error")
        self.assertIn("code 32", status)
        self.assertEqual(lines[-1], "END\t0\t0")

    def test_syntax_error_does_not_run_pylint(self):
        def run(*args, **kwargs):
            self.fail("invalid syntax must be handled before PyLint")
        backend, status, lines = self.analyze(run, "def f(:\n")
        self.assertEqual(backend, "syntax")
        self.assertEqual(status, "")
        self.assertIn("E0001", lines[1])

    def test_extra_paths_are_data_and_available_to_analysis(self):
        sdk = os.path.join(self.project, "SDK été, ;'[]")
        os.mkdir(sdk)
        Path(sdk, "pylint.py").write_text("raise RuntimeError('must not execute')", encoding="utf-8")
        def run(args, reporter, exit):
            self.assertIn(sdk, sys.path)
            self.assertNotIn("--init-hook", " ".join(args))
            self.assertFalse(any(sdk in arg for arg in args))
            self.assertEqual(sys.stdin.read(), "print(inconnu)\n")
        backend, status, _ = self.analyze(run, import_dirs=[sdk])
        self.assertEqual(backend, "pylint")
        self.assertEqual(status, "")

    def test_missing_extra_directory_is_explicit_not_clean(self):
        def run(*args, **kwargs):
            self.fail("missing directory must not be reported as successful analysis")
        backend, status, _ = self.analyze(run, import_dirs=[os.path.join(self.project, "missing")])
        self.assertEqual(backend, "error")
        self.assertIn("Dossier d’import introuvable", status)

    def test_relative_extra_directory_is_rejected(self):
        def run(*args, **kwargs):
            self.fail("relative directory must be resolved by native C first")
        backend, status, _ = self.analyze(run, import_dirs=["relative"])
        self.assertEqual(backend, "error")
        self.assertIn("relative", status)

    def test_too_many_extra_directories_are_rejected(self):
        def run(*args, **kwargs):
            self.fail("excess paths are an invalid configuration")
        backend, status, _ = self.analyze(run, import_dirs=[self.project] * 9)
        self.assertEqual(backend, "error")
        self.assertIn("Liste de dossiers", status)

    def test_capture_is_bounded_and_keeps_last_line(self):
        output = _namespace["_ToolOutput"]()
        text = "é" * 20000
        self.assertEqual(output.write(text), len(text))
        self.assertLessEqual(len(output.getvalue()), 8192)
        output.write("\nlast useful error\n")
        self.assertLessEqual(len(output.getvalue()), 8192)
        self.assertTrue(output.getvalue().endswith("last useful error\n"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
