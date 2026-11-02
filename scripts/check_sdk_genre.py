#!/usr/bin/env python3
"""Жанр не поставляется SDK: явные пути и токены публичных C++ заголовков (#29 В2е)."""
import re
import sys
from pathlib import Path

import py_utf8
from cpp_text import split_code_comments
from sdk_header_names import names_in

WORDS = {'brawl', 'fighter', 'archetype', 'wave', 'checkpoint', 'grip', 'strike', 'combo', 'knock'}
KIT = re.compile(r'^framework/character/(?:controller|assist|ladder|profile[^/]*|trajectory)\.hpp$')
IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')


def tokens(identifier):
    text = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', identifier)
    text = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', text)
    out = set()
    for word in re.findall(r'[A-Za-z][A-Za-z0-9]*', text.lower()):
        out.add(word)
        if word.endswith('s'):
            out.add(word[:-1])
        if word.endswith('es'):
            out.add(word[:-2])
    return out


def genre_words(identifier):
    parts = tokens(identifier)
    found = parts & WORDS
    if {'attack', 'token'} <= parts:
        found.add('attack+token')
    return sorted(found)


def path_problem(path):
    # Читаем ровно engine/<публичный путь>, без абсолютных адресов и алиасов . / .. .
    if not path or path.startswith('/') or any(p in {'', '.', '..'} for p in path.split('/')):
        return 'non-canonical SDK path'
    return None


def audit(headers):
    bad = []
    for path, text in sorted(headers.items()):
        problem = path_problem(path)
        if problem:
            bad.append(f'{path}: {problem}')
            continue
        folded = path.lower()
        if folded.startswith(('framework/brawl/', 'framework/ai/')) or KIT.fullmatch(folded):
            bad.append(f'{path}: forbidden SDK path')
        for word in genre_words(Path(path).stem):
            bad.append(f'{path}: filename token {word}')
        code, _ = split_code_comments(text)
        for line, row in enumerate(code.splitlines(), 1):
            for match in IDENT.finditer(row):
                for word in genre_words(match.group()):
                    bad.append(f'{path}:{line}: {match.group()} contains {word}')
    return bad


def selftest(verbose=True):
    cases = (
        ('plural waves', {'framework/core/x.hpp': 'struct X { int waves; };'}, True),
        ('brawl path', {'framework/brawl/x.hpp': 'struct X {};'}, True),
        ('dot path alias', {'framework/character/./controller.hpp': ''}, True),
        ('parent path alias', {'framework/core/../ai/x.hpp': ''}, True),
        ('absolute path', {'/framework/core/x.hpp': ''}, True),
        ('case alias', {'Framework/Character/Controller.hpp': ''}, True),
        ('ai path', {'framework/ai/x.hpp': 'struct X {};'}, True),
        ('CamelCase fighter', {'framework/core/x.hpp': 'struct FighterTable {};'}, True),
        ('kit controller', {'framework/character/controller.hpp': ''}, True),
        ('kit profile', {'framework/character/profile_bake.hpp': ''}, True),
        ('filename', {'framework/core/checkpoints.hpp': ''}, True),
        ('attack token pair', {'framework/core/x.hpp': 'struct AttackTokens {};'}, True),
        ('TargetArena allowed', {'render/arena.hpp': 'struct TargetArena {};'}, False),
        ('waveform allowed', {'framework/core/waveform.hpp': 'int waveform;'}, False),
        ('comments and literals ignored', {'framework/core/x.hpp': '// FighterTable\nconst char* x = R"(waves Combo)";'}, False),
        ('independent generic words', {'framework/core/x.hpp': 'int attack; int token;'}, False),
    )
    fails = 0
    for name, files, want in cases:
        got = bool(audit(files))
        ok = got == want
        fails += not ok
        if verbose or not ok:
            print(f"  {'ok' if ok else 'FAIL'}: {name}")
    if verbose:
        print(f"sdk-genre selftest: {'PASS' if not fails else 'FAIL'} — {len(cases)} controls")
    return fails


def gate(root):
    source = root / 'cmake/sdk_headers.cmake'
    paths, error = names_in(source.read_text(encoding='utf-8'), 'LIKE_NES_SDK_HEADERS')
    if error or len(paths) < 100 or len(set(paths)) != len(paths):
        print(f'sdk-genre: FAIL — {error or "fewer than 100 unique SDK headers or duplicate paths"}')
        return 1
    headers, missing = {}, []
    engine = (root / 'engine').resolve()
    for path in paths:
        problem = path_problem(path)
        if problem:
            missing.append(f'{path}: {problem}')
            continue
        source = engine / path
        # Символьная ссылка тоже не должна менять каталог или выходить из engine.
        if source.resolve() != source:
            missing.append(f'{path}: aliased SDK source path')
            continue
        try:
            headers[path] = source.read_text(encoding='utf-8')
        except OSError as error:
            missing.append(f'{path}: {error}')
    bad = missing + audit(headers)
    for line in bad:
        print(line)
    print(f"sdk-genre: {'FAIL' if bad else 'PASS'} — {len(headers)} headers, {len(bad)} findings")
    return bool(bad)


def main(argv):
    py_utf8.enable()
    if '--selftest' in argv:
        return bool(selftest())
    if selftest(verbose=False):
        print('sdk-genre: FAIL — broken selftest')
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
