#!/usr/bin/env python3
# Lists every text the app asks to translate and checks gui/i18n_table.inc has a
# Spanish and a Catalan line for each one.
#
#   python3 tools/check_translations.py          # report missing / unused rows
#   python3 tools/check_translations.py --keys   # print the texts found in the code
#
# Found texts: string literals passed to T(...) or TF(...) (also both branches of a
# `cond ? "a" : "b"` inside them), plus the texts translated through tables (condition,
# color and icon names, difficulty and damage-type names, stored messages).
import re, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GUI = os.path.join(ROOT, 'gui')

LIT = r'"((?:[^"\\]|\\.)*)"'

def literals(text):
    """C++ string literals in `text`, joining adjacent ones ("a" "b" -> "ab")."""
    out, i = [], 0
    for m in re.finditer(r'(?:' + LIT + r'\s*)+', text):
        parts = re.findall(LIT, m.group(0))
        out.append(''.join(parts))
    return out

def call_args(src, name):
    """The text inside each name(...) call (balanced parentheses)."""
    for m in re.finditer(r'(?<![\w.])' + name + r'\(', src):
        depth, j = 1, m.end()
        in_str = False
        while j < len(src) and depth:
            c = src[j]
            if in_str:
                if c == '\\': j += 1
                elif c == '"': in_str = False
            elif c == '"': in_str = True
            elif c == '(': depth += 1
            elif c == ')': depth -= 1
            j += 1
        yield src[m.end():j - 1]

def first_argument(arg):
    """`arg` up to its first comma outside strings and parentheses."""
    depth, in_str, i = 0, False, 0
    while i < len(arg):
        c = arg[i]
        if in_str:
            if c == '\\': i += 1
            elif c == '"': in_str = False
        elif c == '"': in_str = True
        elif c in '([': depth += 1
        elif c in ')]': depth -= 1
        elif c == ',' and depth == 0: return arg[:i]
        i += 1
    return arg

def unescape(s):
    return bytes(s, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')

def found_keys():
    keys = set()
    for name in sorted(os.listdir(GUI)):
        if not name.endswith(('.cpp', '.h')) or name.startswith('i18n'):
            continue
        src = open(os.path.join(GUI, name), encoding='utf-8').read()
        for fn in ('T', 'TF'):
            for arg in call_args(src, fn):
                first = first_argument(arg) if fn == 'TF' else arg
                keys.update(unescape(l) for l in literals(first) if re.search(r'[A-Za-z]', l))
    # Texts translated through tables or stored and translated when shown.
    app_h = open(os.path.join(GUI, 'app.h'), encoding='utf-8').read()
    cond_block = app_h[app_h.index('STANDARD_CONDITIONS[]'):app_h.index('};', app_h.index('STANDARD_CONDITIONS[]'))]
    keys.update(literals_list(cond_block))
    looks = open(os.path.join(GUI, 'looks.h'), encoding='utf-8').read()
    pal = looks[looks.index('PALETTE[]'):looks.index('};', looks.index('PALETTE[]'))]
    keys.update(re.findall(r'\{"([^"]+)"', pal))
    icons = looks[looks.index('ICON_NAMES[]'):looks.index('};', looks.index('ICON_NAMES[]'))]
    keys.update(literals_list(icons))
    keys.update(['Trivial', 'Easy', 'Medium', 'Hard', 'Deadly',                 # difficultyName
                 'Normal', 'Resistant', 'Vulnerable', 'Immune',                # damageModName
                 'DEAD', 'DEFEATED',                                           # death labels
                 'Loaded last session.', 'Saved.', 'Could not save.', 'Loaded.', 'Nothing to load.',
                 'Log copied to clipboard.',
                 "Couldn't open a port for the player screens (8080-8099 are all busy)."])
    return keys

def literals_list(block):
    return [unescape(l) for l in re.findall(LIT, block)]

def table_rows():
    src = open(os.path.join(GUI, 'i18n_table.inc'), encoding='utf-8').read()
    rows = {}
    for m in re.finditer(r'\{\s*' + LIT + r'\s*,\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\}', src):
        en, es, ca = (unescape(x) for x in m.groups())
        rows[en] = (es, ca)
    return rows

def main():
    keys = found_keys()
    if '--keys' in sys.argv:
        for k in sorted(keys):
            print(k)
        return 0
    rows = table_rows()
    missing = sorted(k for k in keys if k not in rows)
    unused = sorted(k for k in rows if k not in keys)
    empty = sorted(k for k, (es, ca) in rows.items() if not es or not ca)
    # printf placeholders must match, in order, or TF would read the wrong arguments.
    spec = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?[sdif%]')
    bad_format = []
    for en, (es, ca) in rows.items():
        want = spec.findall(en)
        for lang, text in (('es', es), ('ca', ca)):
            if spec.findall(text) != want:
                bad_format.append((en, lang, text))
    for en, lang, text in bad_format:
        print(f'PLACEHOLDERS differ ({lang}):', repr(en), '->', repr(text))
    for k in missing:
        print('MISSING translation:', repr(k))
    for k in empty:
        print('EMPTY translation:', repr(k))
    for k in unused:
        print('unused row:', repr(k))
    print(f'{len(keys)} texts, {len(rows)} rows, {len(missing)} missing, {len(empty)} empty, {len(unused)} unused')
    return 1 if missing or empty or bad_format else 0

if __name__ == '__main__':
    sys.exit(main())
