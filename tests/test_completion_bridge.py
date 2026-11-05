"""AST and real Odoo source regressions. No framework import/DB/window."""
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

script = Path(sys.argv[1])
REAL_ROOTS = [Path(p) for p in sys.argv[2:]]
sys.argv = [sys.argv[0]]
spec = importlib.util.spec_from_file_location('prisme_completion_test_tool', script)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class CompletionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='prisme-ast-')
        self.base = Path(self.temp.name)
        self.project = self.base / 'projet été'
        self.sdk = self.base / "SDK été, ; ' [racine]"
        self.project.mkdir()
        (self.sdk / 'odoo').mkdir(parents=True)
        self.sentinel = self.base / 'MUST-NOT-EXIST'
        self.put('odoo/__init__.py', 'from . import fields, models, api\nraise RuntimeError("ODoo must never be imported")\n')
        self.put('odoo/fields.py', f'open({str(self.sentinel)!r}, "w").write("EXECUTED")\nclass Char: pass\nclass Integer: pass\nclass Many2one: pass\nclass Date:\n    @staticmethod\n    def today(): pass\n')
        self.put('odoo/models.py', 'class BaseModel:\n    def ensure_one(self): pass\n    def search(self, domain): pass\n    def search_count(self, domain): pass\n    def mapped(self, func): pass\nAbstractModel = BaseModel\nclass Model(AbstractModel): pass\n')
        self.put('odoo/api.py', 'def depends(*args): pass\ndef depends_context(*args): pass\nclass Environment:\n    def ref(self, xmlid): pass\n')
        for name in ['ast.py', 'tokenize.py', 'sitecustomize.py', 'pylint.py']:
            self.put(name, f'open({str(self.sentinel)!r}, "w").write("POISON")\nraise RuntimeError("poisoned tool")\n')
        self.filename = str(self.project / 'modèle.py')
        self.index = tool.Index(str(self.project), [str(self.sdk)])
        (self.project / 'partner.py').write_text('from odoo import models, fields\nclass Partner(models.Model):\n    _name = "res.partner"\n    display_name = fields.Char()\n    partner_code = fields.Char()\n    def action_custom(self): pass\n', encoding='utf-8')
        (self.project / 'extension.py').write_text('from odoo import models, fields\nclass Extend(models.Model):\n    _inherit = "res.partner"\n    custom_flag = fields.Char()\n', encoding='utf-8')

    def put(self, name, text):
        path = self.sdk / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')

    def tearDown(self):
        self.assertFalse(self.sentinel.exists(), 'Framework/tool code was executed!')
        self.temp.cleanup()

    def query(self, source, index=None):
        cursor = source.index('|') if '|' in source else len(source)
        source = source.replace('|', '', 1)
        return (index or self.index).query(source, self.filename, len(source[:cursor].encode('utf-8')))

    def names(self, source, index=None):
        return [item[1] for item in self.query(source, index)[2]]

    def model(self, tail):
        return ('from odoo import fields, models, api\nclass Demo(models.Model):\n'
                '    _name = "demo.model"\n    title = fields.Char()\n'
                '    partner_id = fields.Many2one("res.partner")\n'
                '    def action_demo(self):\n        ' + tail)

    def test_module_export_and_package_self_alias(self):
        self.assertIn('fields', self.names('from odoo import fi'))
        self.assertIn('Char', self.names('from odoo import fields\nfields.Ch'))
        self.assertIn('Model', self.names('from odoo import models\nmodels.Mo'))

    def test_import_aliases(self):
        self.assertIn('Char', self.names('from odoo import fields as f\nf.Ch'))
        self.assertIn('Char', self.names('import odoo as o\no.fields.Ch'))
        self.assertIn('today', self.names('from odoo.fields import Date as D\nD.to'))

    def test_decorator(self):
        self.assertIn('depends', self.names('from odoo import api\n@api.dep'))

    def test_core_recordset_method(self):
        self.assertIn('ensure_one', self.names(self.model('self.ens')))
        self.assertIn('search', self.names(self.model('self.se')))

    def test_unsaved_field(self):
        self.assertIn('title', self.names(self.model('self.ti')))
        self.assertFalse(Path(self.filename).exists())

    def test_environment(self):
        self.assertIn('ref', self.names(self.model('self.env.re')))

    def test_odoo_model_name_string(self):
        source = self.model("self.env['res.p|']")
        start, end, items, _ = self.query(source)
        self.assertIn('res.partner', [item[1] for item in items])
        plain = source.replace('|', '')
        self.assertEqual(plain.encode()[start:end].decode(), 'res.p')

    def test_model_members_and_inherited_addon_fields(self):
        self.assertIn('display_name', self.names(self.model("self.env['res.partner'].dis")))
        self.assertIn('custom_flag', self.names(self.model("self.env['res.partner'].custom_")))
        self.assertIn('action_custom', self.names(self.model("self.env['res.partner'].action_")))

    def test_relational_field(self):
        self.assertIn('display_name', self.names(self.model('self.partner_id.dis')))

    def test_search_recordset_chain(self):
        self.assertIn('display_name', self.names(self.model("self.env['res.partner'].search([]).dis")))

    def test_local_recordset_alias(self):
        self.assertIn('display_name', self.names(self.model("partners = self.env['res.partner']\n        partners.dis")))

    def test_recordset_iteration(self):
        self.assertIn('title', self.names(self.model('for record in self:\n            record.ti')))

    def test_bare_import_prefix(self):
        self.assertIn('fields', self.names('import odoo.fi'))

    def test_utf8_byte_ranges_and_midword_suffix(self):
        source = '# été\nfrom odoo import fields\nfields.Ch|ar_bad'
        start, end, items, _ = self.query(source)
        raw = source.replace('|', '').encode()
        self.assertEqual(raw[start:end].decode(), 'Char_bad')
        self.assertIn('Char', [item[1] for item in items])

    def test_no_exact_word_popup_except_native_macros(self):
        self.assertNotIn('Char', self.names('from odoo import fields\nfields.Char'))

    def test_stub_precedence(self):
        self.put('odoo/fields.pyi', 'class CustomStubField: ...\n')
        self.assertIn('CustomStubField', self.names('from odoo import fields\nfields.Custom'))
        self.assertNotIn('Char', self.names('from odoo import fields\nfields.Ch'))

    def test_namespace_packages(self):
        (self.sdk / 'odoo/__init__.py').unlink()
        self.assertIn('Char', self.names('from odoo import fields\nfields.Ch'))

    def test_cache_and_source_change(self):
        self.names('from odoo import fields\nfields.Ch')
        key = ('odoo.fields', str(self.sdk / 'odoo/fields.py'))
        old = self.index.cache[key]
        self.names('from odoo import fields\nfields.In')
        self.assertIs(old, self.index.cache[key])
        self.put('odoo/fields.py', 'class ChangedField: pass\n')
        self.assertIn('ChangedField', self.names('from odoo import fields\nfields.Changed'))
        self.assertIsNot(old, self.index.cache[key])

    def test_unavailable_directory_explicit(self):
        self.index.roots = [str(self.sdk / 'missing')]
        with self.assertRaisesRegex(ValueError, 'introuvable'):
            self.names('from odoo import fields\nfields.Ch')

    def test_root_removal(self):
        self.assertIn('Char', self.names('from odoo import fields\nfields.Ch'))
        self.index.roots = []
        self.assertNotIn('Char', self.names('from odoo import fields\nfields.Ch'))

    def test_missing_module_never_executed_to_discover_attributes(self):
        self.put('dynamic.py', f'def __getattr__(name):\n    open({str(self.sentinel)!r}, "w").write(name)\n')
        self.assertEqual(self.names('import dynamic\ndynamic.unknown'), [])

    def test_syntax_error_elsewhere_keeps_imports(self):
        self.assertIn('Char', self.names('from odoo import fields\nif broken ???\nfields.Ch'))

    def test_other_function_locals_do_not_leak(self):
        source = self.model('pass\n    def second(self):\n        hidden_local = self.env["res.partner"]\n    def third(self):\n        hidden_')
        self.assertNotIn('hidden_local', self.names(source))

    def test_nested_function_locals_do_not_leak(self):
        source = self.model('def nested():\n            nested_partner = self.env["res.partner"]\n        nested_p')
        self.assertNotIn('nested_partner', self.names(source))

    def test_invalid_cursor_and_config(self):
        with self.assertRaises(ValueError):
            tool.Index('relative', [])
        with self.assertRaises(ValueError):
            tool.Index(str(self.project), [str(self.sdk)] * 9)
        with self.assertRaises(UnicodeError):
            self.index.query('é', self.filename, 1)
        with self.assertRaises(ValueError):
            self.index.query('x', self.filename, 999)

    def test_catalog_time_budget_and_persistent_partial_status(self):
        with mock.patch.object(tool, '_MAX_CATALOG_SECONDS', 0):
            result = self.query(self.model("self.env['res.p|']"))
        self.assertIn('partiel', result[3])
        self.assertIn('partiel', self.query(self.model("self.env['res.p|']"))[3])

    def test_directory_listing_is_bounded(self):
        self.index.search = [str(self.sdk)]
        with mock.patch.object(tool, '_MAX_DIRECTORY_ENTRIES', 2):
            self.assertLessEqual(len(self.index.children('')), 2)
        self.assertIn('partiels', self.index.status)

    def test_reexport_kind_resolved(self):
        (self.sdk / 'odoo/fields.py').unlink()
        self.put('odoo/fields/__init__.py', 'from ..orm.real_fields import Char\n')
        self.put('odoo/orm/real_fields.py', 'class Char: pass\n')
        item = self.query('from odoo import fields\nfields.Ch')[2][0]
        self.assertEqual(item[:2], ('C', 'Char'))

    def test_syntax_warning_cannot_corrupt_worker_protocol(self):
        self.put('odoo/fields.py', 'REGEX = "\\d"\nclass Char: pass\n')
        with mock.patch.object(sys, 'stderr', io.StringIO()) as errors:
            self.assertIn('Char', self.names('from odoo import fields\nfields.Ch'))
        self.assertEqual(errors.getvalue(), '')

    def test_protocol_is_lf_only_on_windows_too(self):
        raw = io.BytesIO()
        wrapper = io.TextIOWrapper(raw, encoding='cp1252', newline='\r\n')
        with mock.patch.object(sys, '__stdout__', wrapper):
            tool._response(9, 1, 2, [('C', 'Char', 'Classe · été')], 'statut\nété')
        data = raw.getvalue()
        self.assertNotIn(b'\r', data)
        self.assertTrue(data.endswith(b'END\t9\t1\n'))
        self.assertIn('Classe · été'.encode().hex().encode(), data)
        wrapper.detach()

    def test_catalog_file_budget(self):
        with mock.patch.object(tool, '_MAX_CATALOG_FILES', 1):
            self.assertIn('partiel', self.query(self.model("self.env['res.p|']"))[3])

    def test_source_size_limit(self):
        with mock.patch.object(tool, '_MAX_FILE', 10):
            with self.assertRaises(ValueError):
                self.names('from odoo import fields\nfields.Ch')

    @unittest.skipUnless(REAL_ROOTS, 'real Odoo source fixture paths not configured')
    def test_actual_odoo_sources(self):
        for root in REAL_ROOTS:
            index = tool.Index(str(self.project), [str(root)])
            with self.subTest(root=str(root)):
                self.assertIn('Char', self.names('from odoo import fields\nfields.Ch', index))
                self.assertIn('Model', self.names('from odoo import models\nmodels.Mo', index))
                self.assertIn('depends', self.names('from odoo import api\n@api.dep', index))
                self.assertIn('ensure_one', self.names(self.model('self.ens'), index))
                self.assertIn('title', self.names(self.model('self.ti'), index))
                print('OK actual Odoo static source:', root.name, 'Char/Model/depends/ensure_one/unsaved field; no imports/DB')


unittest.main(verbosity=2)
