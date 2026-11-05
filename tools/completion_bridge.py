"""Read-only, persistent AST completion service for the native C editor.

Only Python's standard library is imported. Framework/project code is READ and
parsed, never imported, evaluated or executed. No database or Odoo installation
is needed. C owns the private request file and all process/version lifetimes.
"""
import ast
from itertools import islice
import os
import re
import sys
import tokenize
import time
import warnings

_MAX_FILE = 2 * 1024 * 1024
_MAX_CATALOG_FILES = 4096
_MAX_CATALOG_BYTES = 64 * 1024 * 1024
_MAX_MEMBERS = 8192
_MAX_DIRECTORY_ENTRIES = 4096
_MAX_CATALOG_SECONDS = 2.5
_SKIP = {'.git', '.hg', '.svn', '.venv', 'venv', 'node_modules', '__pycache__',
         '.mypy_cache', '.pytest_cache', '.tox', 'build', 'dist'}
_RECORDSET_CALLS = {'browse', 'search', 'search_fetch', 'create', 'copy', 'new',
                    'sudo', 'with_context', 'with_user', 'with_company', 'with_env',
                    'exists', 'filtered', 'filtered_domain', 'sorted', 'ensure_one'}


def _parse(source, **kwargs):
    # Python can emit SyntaxWarning while merely parsing old regex literals.
    # Keep stderr/stdout a strict protocol; diagnostics use the separate linter.
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', SyntaxWarning)
        warnings.simplefilter('ignore', DeprecationWarning)
        return ast.parse(source, **kwargs)


def _local_nodes(body):
    """Walk one lexical function scope, excluding nested functions/classes."""
    for node in body:
        yield node
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef, ast.Lambda)):
            yield from _local_nodes(ast.iter_child_nodes(node))


class Symbol:
    def __init__(self, name, kind, unit, value=None, detail=''):
        self.name, self.kind, self.unit = name, kind, unit
        self.value, self.detail = value, detail
        self.members, self.bases = {}, []
        self.model, self.inherits, self.target = '', [], ''


class Unit:
    def __init__(self, module, path='', package=False):
        self.module, self.path, self.package = module, path, package
        self.members, self.stars = {}, []
        self.stamp = None
        self.classes = []


def _string(node):
    return node.value if isinstance(node, ast.Constant) and isinstance(node.value, str) else ''


def _strings(node):
    if _string(node):
        return [_string(node)]
    if isinstance(node, (ast.List, ast.Tuple)):
        return [s for item in node.elts if (s := _string(item))]
    return []


def _statements(body):
    """Declarations in conditional/type-checking blocks, never function bodies."""
    for node in body:
        yield node
        if isinstance(node, ast.If):
            yield from _statements(node.body)
            yield from _statements(node.orelse)
        elif isinstance(node, (ast.Try, getattr(ast, 'TryStar', ast.Try))):
            yield from _statements(node.body)
            yield from _statements(node.orelse)
            for handler in node.handlers:
                yield from _statements(handler.body)


def _absolute_import(unit, node):
    if not node.level:
        return node.module or ''
    base = unit.module.split('.') if unit.package else unit.module.split('.')[:-1]
    if node.level > len(base):
        return ''
    base = base[:len(base) - node.level + 1]
    if node.module:
        base.extend(node.module.split('.'))
    return '.'.join(base)


def _build_unit(tree, module, path='', package=False):
    unit = Unit(module, path, package)

    def collect(body, members, owner=None):
        for node in _statements(body):
            if len(members) >= _MAX_MEMBERS:
                break
            if isinstance(node, ast.ClassDef):
                symbol = Symbol(node.name, 'class', unit, detail='Classe · ' + module)
                symbol.bases = node.bases
                collect(node.body, symbol.members, symbol)
                unit.classes.append(symbol)
                members[node.name] = symbol
            elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                names = [a.arg for a in node.args.args]
                if owner and names and names[0] in ('self', 'cls'):
                    names = names[1:]
                detail = ('Méthode' if owner else 'Fonction') + ' · ' + node.name + '(' + ', '.join(names[:8]) + ')'
                symbol = Symbol(node.name, 'function', unit, node.returns, detail)
                members[node.name] = symbol
            elif isinstance(node, ast.Import):
                for alias in node.names:
                    name = alias.asname or alias.name.split('.')[0]
                    target = alias.name if alias.asname else name
                    members[name] = Symbol(name, 'module', unit, target, 'Module · ' + target)
            elif isinstance(node, ast.ImportFrom):
                module_name = _absolute_import(unit, node)
                for alias in node.names:
                    if alias.name == '*':
                        unit.stars.append(module_name)
                        continue
                    name = alias.asname or alias.name
                    members[name] = Symbol(name, 'alias', unit, (module_name, alias.name),
                                           'Import · ' + module_name + '.' + alias.name)
            elif isinstance(node, (ast.Assign, ast.AnnAssign)):
                targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                value = node.value
                for target in targets:
                    if not isinstance(target, ast.Name):
                        continue
                    name = target.id
                    symbol = Symbol(name, 'value', unit, value, 'Attribut · ' + module)
                    if isinstance(value, ast.Call) and isinstance(value.func, ast.Attribute):
                        field_name = value.func.attr
                        if field_name in {'Many2one', 'Many2many', 'One2many'}:
                            symbol.target = _string(value.args[0]) if value.args else ''
                            for kw in value.keywords:
                                if kw.arg == 'comodel_name':
                                    symbol.target = _string(kw.value)
                        if owner:
                            symbol.detail = 'Champ · ' + field_name
                    if owner and name == '_name':
                        owner.model = _string(value)
                    elif owner and name == '_inherit':
                        owner.inherits = _strings(value)
                    members[name] = symbol
    collect(tree.body, unit.members)
    return unit


class Index:
    def __init__(self, project, roots):
        if len(roots) > 8 or not os.path.isabs(project):
            raise ValueError('Configuration de complétion invalide.')
        self.project = project
        self.roots = list(roots)
        self.search = []
        self.cache = {}
        self.catalog = {}
        self.catalog_ready = False
        self.catalog_status = ''
        self.status = ''
        self.document = None
        self.current_class = None
        self.locals = {}

    def _paths(self, filename):
        for path in self.roots:
            if not isinstance(path, str) or not os.path.isabs(path) or not os.path.isdir(path):
                raise ValueError('Dossier d’import introuvable ou inaccessible : ' + str(path))
        self.search = list(dict.fromkeys([self.project, os.path.dirname(filename)] + self.roots))

    def _location(self, module):
        if not module or not all(p.isidentifier() for p in module.split('.')):
            return None
        parts = module.split('.')
        namespaces = []
        for root in self.search:
            base = os.path.join(root, *parts)
            for suffix, package in [('/__init__.pyi', True), ('/__init__.py', True),
                                    ('.pyi', False), ('.py', False)]:
                path = base + suffix.replace('/', os.sep)
                if os.path.isfile(path):
                    return path, package
            if os.path.isdir(base):
                namespaces.append(base)
        return (namespaces[0], True) if namespaces else None

    def unit(self, module):
        where = self._location(module)
        if not where:
            return None
        path, package = where
        if os.path.isdir(path):
            return Unit(module, path, True)
        stamp = os.stat(path).st_mtime_ns, os.stat(path).st_size
        key = module, path
        previous = self.cache.get(key)
        if previous and previous.stamp == stamp:
            return previous
        if stamp[1] > _MAX_FILE:
            self.status = 'Source ignorée (> 2 Mo) : ' + path
            return None
        try:
            with tokenize.open(path) as f:
                text = f.read(_MAX_FILE + 1)
            if len(text.encode('utf-8')) > _MAX_FILE:
                return None
            tree = _parse(text, filename=path)
        except (OSError, SyntaxError, UnicodeError, ValueError, RecursionError) as error:
            self.status = 'Source non analysable avec ce Python : ' + os.path.basename(path) + ' (' + type(error).__name__ + ')'
            return None
        unit = _build_unit(tree, module, path, package)
        unit.stamp = stamp
        if len(self.cache) >= 1024:
            self.cache.pop(next(iter(self.cache)))
        self.cache[key] = unit
        return unit

    def children(self, module):
        where = self._location(module) if module else (None, True)
        if module and not where:
            return {}
        paths = self.search if not module else [where[0] if os.path.isdir(where[0]) else os.path.dirname(where[0])] if where[1] else []
        result = {}
        for folder in paths:
            try:
                with os.scandir(folder) as scan:
                    entries = list(islice(scan, _MAX_DIRECTORY_ENTRIES + 1))
                if len(entries) > _MAX_DIRECTORY_ENTRIES:
                    self.status = 'Modules partiels : limite de 4 096 entrées par dossier.'
                entries = sorted(entries[:_MAX_DIRECTORY_ENTRIES], key=lambda e: e.name)
                for entry in entries:
                    name = entry.name
                    if entry.is_dir(follow_symlinks=False):
                        if name in _SKIP or not name.isidentifier():
                            continue
                    elif entry.is_file(follow_symlinks=False) and name.endswith(('.py', '.pyi')):
                        name = name.rsplit('.', 1)[0]
                        if not name.isidentifier() or name == '__init__':
                            continue
                    else:
                        continue
                    full = (module + '.' if module else '') + name
                    result[name] = Symbol(name, 'module', Unit(module), full, 'Module · ' + full)
            except OSError:
                continue
        return result

    def symbol(self, symbol, depth=0):
        if symbol is None or depth > 16:
            return None
        if symbol.kind in ('class', 'function'):
            return symbol.kind, symbol
        if symbol.kind == 'module':
            return 'module', symbol.value
        if symbol.kind == 'alias':
            module, name = symbol.value
            unit = self.unit(module)
            if unit and name in unit.members and unit.members[name] is not symbol:
                resolved = self.symbol(unit.members[name], depth + 1)
                if resolved:
                    return resolved
            if self._location(module + '.' + name):
                return 'module', module + '.' + name
            if unit:
                for star in unit.stars:
                    other = self.unit(star)
                    if other and name in other.members:
                        return self.symbol(other.members[name], depth + 1)
            return None
        if symbol.target:
            return 'model', symbol.target
        return self.resolve(symbol.value, symbol.unit, {}, depth + 1) if symbol.value else None

    def members(self, ref, depth=0, seen=None):
        if not ref or depth > 12:
            return {}
        seen = set() if seen is None else set(seen)
        kind, value = ref
        tag = kind, value if isinstance(value, str) else id(value)
        if tag in seen:
            return {}
        seen.add(tag)
        result = {}
        if kind == 'module':
            unit = self.unit(value)
            result.update(self.children(value))
            if unit:
                for star in unit.stars:
                    result.update(self.members(('module', star), depth + 1, seen))
                result.update(unit.members)
        elif kind in ('class', 'instance'):
            for base in value.bases:
                result.update(self.members(self.resolve(base, value.unit, {}, depth + 1), depth + 1, seen))
            for model in value.inherits:
                result.update(self.members(('model', model), depth + 1, seen))
            result.update(value.members)
        elif kind == 'model':
            result.update(self.members(self.odoo_class('BaseModel'), depth + 1, seen))
            self.model_catalog()
            for cls in self.catalog.get(value, []):
                result.update(self.members(('instance', cls), depth + 1, seen))
            if self.current_class and (self.current_class.model == value or value in self.current_class.inherits):
                result.update(self.current_class.members)
        elif kind == 'environment':
            result.update(self.members(self.odoo_environment(), depth + 1, seen))
        return result

    def odoo_class(self, name):
        for module in ('odoo.models', 'odoo.orm.models'):
            unit = self.unit(module)
            if unit and name in unit.members:
                return self.symbol(unit.members[name])
        return None

    def odoo_environment(self):
        for module in ('odoo.api', 'odoo.orm.environments'):
            unit = self.unit(module)
            if unit and 'Environment' in unit.members:
                return self.symbol(unit.members['Environment'])
        return None

    def is_odoo(self, ref, depth=0):
        if not ref or depth > 8:
            return False
        if ref[0] == 'model':
            return True
        if ref[0] not in ('class', 'instance'):
            return False
        cls = ref[1]
        if cls.unit.module in ('odoo.models', 'odoo.orm.models') and cls.name in ('BaseModel', 'Model', 'AbstractModel', 'TransientModel'):
            return True
        return bool(cls.model or cls.inherits) or any(self.is_odoo(self.resolve(b, cls.unit, {}, depth + 1), depth + 1) for b in cls.bases)

    def resolve(self, node, unit=None, local=None, depth=0):
        if node is None or depth > 16:
            return None
        unit = unit or self.document
        local = self.locals if local is None else local
        if isinstance(node, ast.Name):
            if node.id in local:
                value = local[node.id]
                return value if isinstance(value, tuple) else self.resolve(value, unit, {}, depth + 1)
            symbol = unit.members.get(node.id) if unit else None
            return self.symbol(symbol, depth + 1)
        if isinstance(node, ast.Attribute):
            owner = self.resolve(node.value, unit, local, depth + 1)
            if not owner:
                return None
            if node.attr == 'env' and self.is_odoo(owner):
                return 'environment', None
            if owner[0] == 'environment' and node.attr in ('user', 'company', 'companies'):
                return 'model', 'res.users' if node.attr == 'user' else 'res.company'
            symbol = self.members(owner).get(node.attr)
            return self.symbol(symbol, depth + 1)
        if isinstance(node, ast.Subscript):
            owner = self.resolve(node.value, unit, local, depth + 1)
            sub = node.slice.value if hasattr(ast, 'Index') and isinstance(node.slice, ast.Index) else node.slice
            if owner and owner[0] == 'environment' and _string(sub):
                return 'model', _string(sub)
            if owner and owner[0] == 'class':
                return owner
        if isinstance(node, ast.Call):
            if isinstance(node.func, ast.Attribute):
                owner = self.resolve(node.func.value, unit, local, depth + 1)
                if owner and self.is_odoo(owner) and node.func.attr in _RECORDSET_CALLS:
                    return owner
            function = self.resolve(node.func, unit, local, depth + 1)
            if function and function[0] == 'class':
                return 'instance', function[1]
        return None

    def model_catalog(self):
        if self.catalog_ready:
            if self.catalog_status:
                self.status = self.catalog_status
            return
        self.catalog_ready = True
        deadline = time.monotonic() + _MAX_CATALOG_SECONDS
        folders = [self.project]
        for root in self.roots:
            if os.path.isdir(os.path.join(root, 'odoo')):
                folders.extend([os.path.join(root, 'odoo', 'addons'), os.path.join(root, 'addons')])
            else:
                folders.append(root)

        def partial(reason):
            self.status = self.catalog_status = 'Index Odoo partiel : ' + reason + ' Choisissez des dossiers plus ciblés ou réindexez.'

        def sources(folder):
            pending = [folder]
            directories = 0
            while pending:
                if time.monotonic() >= deadline:
                    partial('budget de 2,5 secondes atteint.')
                    return
                current = pending.pop()
                directories += 1
                if directories > 4096:
                    partial('limite de 4 096 sous-dossiers.')
                    return
                try:
                    with os.scandir(current) as scan:
                        entries = list(islice(scan, _MAX_DIRECTORY_ENTRIES + 1))
                    if len(entries) > _MAX_DIRECTORY_ENTRIES:
                        partial('limite de 4 096 entrées par dossier.')
                    dirs = []
                    for entry in sorted(entries[:_MAX_DIRECTORY_ENTRIES], key=lambda e: e.name):
                        if time.monotonic() >= deadline:
                            partial('budget de 2,5 secondes atteint.')
                            return
                        if entry.is_dir(follow_symlinks=False):
                            if entry.name not in _SKIP and not entry.name.startswith('.'):
                                dirs.append(entry.path)
                        elif entry.is_file(follow_symlinks=False) and entry.name.endswith(('.py', '.pyi')):
                            yield entry.name, entry.path
                    pending.extend(reversed(dirs))
                    if len(pending) > 4096:
                        partial('limite de 4 096 sous-dossiers en attente.')
                        pending = pending[-4096:]
                except OSError:
                    continue

        seen, files, size = set(), 0, 0
        for folder in dict.fromkeys(folders):
            if not os.path.isdir(folder):
                continue
            for name, path in sources(folder):
                if path in seen:
                    continue
                seen.add(path)
                try:
                    n = os.stat(path).st_size
                    if n > _MAX_FILE:
                        continue
                    files += 1
                    size += n
                    if files > _MAX_CATALOG_FILES or size > _MAX_CATALOG_BYTES:
                        partial('limite de 4 096 fichiers / 64 Mo.')
                        return
                    with tokenize.open(path) as f:
                        text = f.read(_MAX_FILE + 1)
                    if len(text.encode('utf-8')) > _MAX_FILE:
                        continue
                    unit = _build_unit(_parse(text, filename=path), self.module_for(path), path, name.startswith('__init__.'))
                    for cls in unit.classes:
                        keys = [cls.model] if cls.model else cls.inherits
                        for model in keys:
                            self.catalog.setdefault(model, []).append(cls)
                except (OSError, UnicodeError, SyntaxError, ValueError, RecursionError):
                    continue

    def module_for(self, filename):
        for root in self.search:
            try:
                rel = os.path.relpath(filename, root)
            except ValueError:
                continue
            if rel != '..' and not rel.startswith('..' + os.sep):
                parts = rel.replace('\\', '/').rsplit('.', 1)[0].split('/')
                if parts[-1] == '__init__':
                    parts.pop()
                if all(p.isidentifier() for p in parts):
                    return '.'.join(parts)
        return '_document'

    def prepare(self, source, filename, cursor, start, end):
        # Repair incomplete cursor tokens without executing or saving the source.
        repaired = source[:start] + '_prisme_cursor' + source[end:]
        row = source[:cursor].count('\n') + 1
        lines = repaired.splitlines(True)
        tree = None
        for attempt in range(12):
            try:
                tree = _parse(''.join(lines), filename=filename)
                break
            except (SyntaxError, ValueError) as error:
                line = max(1, min(len(lines), getattr(error, 'lineno', None) or row)) - 1
                if not lines:
                    break
                text = lines[line]
                indent = text[:len(text) - len(text.lstrip())]
                if text.lstrip().startswith('@') and attempt == 0:
                    lines[line] = lines[line].rstrip('\r\n') + '\n'
                    lines.insert(line + 1, indent + 'def _prisme_decorated(): pass\n')
                else:
                    lines[line] = indent + 'pass\n'
            except RecursionError:
                break
        if tree is None:
            # Retain valid import statements even in a temporarily broken file.
            body = []
            for text in source.splitlines():
                if re.match(r'^\s*(from|import)\s+', text):
                    try:
                        body.extend(_parse(text.strip()).body)
                    except SyntaxError:
                        pass
            tree = ast.Module(body=body, type_ignores=[])
        self.document = _build_unit(tree, self.module_for(filename), filename)
        self.current_class, self.locals = None, {}
        active_class = None
        active_function = None
        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef) and node.lineno <= row <= getattr(node, 'end_lineno', node.lineno):
                active_class = node
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.lineno <= row <= getattr(node, 'end_lineno', node.lineno):
                active_function = node
        if active_class:
            self.current_class = next((c for c in self.document.classes if c.name == active_class.name), None)
        if self.current_class and active_function and active_function.args.args:
            self.locals[active_function.args.args[0].arg] = ('instance', self.current_class)
        if active_function:
            for node in _local_nodes(active_function.body):
                if getattr(node, 'lineno', row + 1) > row:
                    continue
                if isinstance(node, (ast.Assign, ast.AnnAssign)):
                    targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                    ref = self.resolve(node.value)
                    for target in targets:
                        if isinstance(target, ast.Name) and ref:
                            self.locals[target.id] = ref
                elif isinstance(node, (ast.For, ast.AsyncFor)) and isinstance(node.target, ast.Name):
                    ref = self.resolve(node.iter)
                    if ref:
                        self.locals[node.target.id] = ref

    def query(self, source, filename, cursor_bytes):
        self._paths(filename)
        self.status = ''
        raw = source.encode('utf-8')
        if len(raw) > _MAX_FILE or not 0 <= cursor_bytes <= len(raw):
            raise ValueError('Document trop volumineux ou position de complétion invalide.')
        cursor = len(raw[:cursor_bytes].decode('utf-8'))
        start, end = cursor, cursor
        while start and (source[start - 1].isalnum() or source[start - 1] == '_'):
            start -= 1
        while end < len(source) and (source[end].isalnum() or source[end] == '_'):
            end += 1
        prefix = source[start:cursor]
        before = source[:cursor]
        self.prepare(source, filename, cursor, start, end)
        result = {}
        model_string = re.search(r'\b[A-Za-z_]\w*\.env\s*\[\s*([\'\"])([^\'\"\n]*)$', before[-4096:])
        if model_string:
            prefix = model_string.group(2)
            start = cursor - len(prefix)
            end = cursor
            while end < len(source) and source[end] not in "'\"\n]":
                end += 1
            self.model_catalog()
            names = dict(self.catalog)
            if self.current_class:
                for model in ([self.current_class.model] if self.current_class.model else self.current_class.inherits):
                    names[model] = []
            result = {name: Symbol(name, 'model', self.document, detail='Modèle Odoo · sources statiques') for name in names}
        else:
            line = before.rsplit('\n', 1)[-1]
            from_import = re.search(r'\bfrom\s+([\w.]+)\s+import\s+(?:[\w]+\s*,\s*)*(\w*)$', line)
            import_module = re.search(r'\bimport\s+([\w.]*)$', line)
            if from_import:
                module = from_import.group(1)
                if module.startswith('.'):
                    dummy = ast.ImportFrom(module=module.lstrip('.') or None, names=[], level=len(module) - len(module.lstrip('.')))
                    module = _absolute_import(self.document, dummy)
                result = self.members(('module', module))
            elif import_module:
                full = import_module.group(1)
                parent = full.rsplit('.', 1)[0] if '.' in full else ''
                result = self.children(parent)
            elif start and source[start - 1] == '.':
                expression = source[max(0, start - 4096):start - 1].rsplit('\n', 1)[-1]
                for match in re.finditer(r'\b[A-Za-z_]\w*', expression):
                    try:
                        node = _parse(expression[match.start():], mode='eval').body
                    except SyntaxError:
                        continue
                    ref = self.resolve(node)
                    if ref:
                        result = self.members(ref)
                        break
            else:
                result.update(self.document.members)
                for name, ref in self.locals.items():
                    result[name] = Symbol(name, 'value', self.document, detail='Variable locale')
        names = [name for name in result if name != prefix and name.startswith(prefix) and (not name.startswith('_') or prefix.startswith('_')) and len(name.encode('utf-8')) < 96]
        names.sort(key=lambda name: (name != prefix, name.lower(), name))
        items = []
        for name in names[:32]:
            symbol = result[name]
            # Export aliases remain useful even when an optional target source
            # is absent. Resolving them is needed only for deeper attributes.
            resolved = self.symbol(symbol) if symbol.kind == 'alias' else None
            symbol_kind = resolved[0] if resolved else symbol.kind
            kind = {'class': 'C', 'module': 'M', 'function': 'F', 'model': 'O'}.get(symbol_kind, 'V')
            detail = symbol.detail or 'Symbole · ' + symbol.unit.module
            items.append((kind, name, detail))
        return len(source[:start].encode('utf-8')), len(source[:end].encode('utf-8')), items, self.status


def _hex(text, limit):
    text = ''.join(c if ord(c) >= 32 and ord(c) != 127 else ' ' for c in str(text))
    return text.encode('utf-8')[:limit].decode('utf-8', 'ignore').encode('utf-8').hex()


def _response(sequence, start, end, items, status='', failed=False):
    lines = ['PRISME-COMPLETE\t1\t%d\t%d\t%d\t%d\t%s' % (sequence, start, end, int(failed), _hex(status, 383))]
    for kind, name, detail in items:
        lines.append('C\t%s\t%s\t%s' % (kind, _hex(name, 95), _hex(detail, 255)))
    lines.append('END\t%d\t%d' % (sequence, len(items)))
    # Binary ASCII framing is LF-only even with a Windows CRLF text wrapper.
    sys.__stdout__.buffer.write(('\n'.join(lines) + '\n').encode('ascii'))
    sys.__stdout__.buffer.flush()


def _prisme_completion_serve(request_file, project, roots):
    index = Index(project, roots)
    for command in sys.stdin:
        sequence = 0
        try:
            if len(command) > 64 or not re.fullmatch(r'Q\t[0-9]+\n', command):
                raise ValueError('Commande de complétion invalide.')
            sequence = int(command[2:].strip())
            with open(request_file, 'rb') as f:
                raw = f.read(_MAX_FILE + 20000)
            header, body = raw.split(b'\n', 1)
            magic, version, token, position, filename = header.decode('ascii').split('\t')
            if magic != 'PRISME-COMPLETE-REQUEST' or version != '1' or int(token) != sequence or len(body) > _MAX_FILE:
                raise ValueError('Requête de complétion invalide.')
            filename = bytes.fromhex(filename).decode('utf-8')
            if not os.path.isabs(filename) or len(filename.encode('utf-8')) >= 4096:
                raise ValueError('Nom de document invalide.')
            start, end, items, status = index.query(body.decode('utf-8'), filename, int(position))
            _response(sequence, start, end, items, status)
        except Exception as error:
            _response(sequence, 0, 0, [], 'Complétion indisponible : ' + str(error), True)
