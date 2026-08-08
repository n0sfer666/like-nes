import atexit, re, os, hashlib, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCK = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    ROOT, 'build-ios/_deps/wgpu_native_src-src/Cargo.lock')
REGROOT = os.path.expanduser('~/.cargo/registry/src')
OUT = os.path.join(ROOT, 'THIRD-PARTY-NOTICES-RUST.txt')

if not os.path.isfile(LOCK):
    sys.exit(f'Cargo.lock не найден: {LOCK} — сконфигурируйте мобильную сборку wgpu-native')
regs = sorted(os.path.join(REGROOT, d) for d in os.listdir(REGROOT)
              if os.path.isdir(os.path.join(REGROOT, d))) if os.path.isdir(REGROOT) else []
if not regs:
    sys.exit('локальный cargo-реестр пуст — выполните cargo fetch --locked рядом с Cargo.lock')

def crate_dir(n, v):
    for r in regs:
        d = os.path.join(r, f'{n}-{v}')
        if os.path.isdir(d):
            return d
    return None

WORKSPACE_SPDX = 'MIT OR Apache-2.0'
WORKSPACE_CRATES = {'wgpu-core', 'wgpu-hal', 'wgpu-types', 'wgpu-native', 'naga', 'd3d12'}
GITROOT = os.path.expanduser('~/.cargo/git/checkouts')

# CONTRIBUTING объявляет copyleft и source-available неприемлемыми, и этот скрипт — единственный
# автоматизированный шаг, который может это заметить. Нераспознанный идентификатор (GPL, BSL,
# «unstated» при license.workspace = true) обязан ронять регенерацию, а не уезжать в артефакт.
ALLOWED_SPDX = {'MIT', 'MIT-0', 'Apache-2.0', 'BSD-2-Clause', 'BSD-3-Clause', 'ISC', 'Zlib',
                'Unlicense', 'CC0-1.0', 'BSL-1.0', 'Unicode-DFS-2016', 'Unicode-3.0',
                'LLVM-exception'}

def check_spdx(name, version, expr):
    toks = [t for t in expr.replace('(', ' ').replace(')', ' ').split()
            if t not in ('OR', 'AND', 'WITH')]
    bad = [t for t in toks if t not in ALLOWED_SPDX]
    if bad:
        sys.exit(f'крейт {name} {version}: лицензия "{expr}" не в списке разрешённых '
                 f'(нераспознано: {", ".join(bad)}) — notices не перезаписаны')
    return expr

# Крейты воркспейса wgpu приезжают git-зависимостью и в registry/src не попадают, поэтому их
# лицензия читается из дерева исходников. Не нашли — говорим об этом вслух и в самом артефакте,
# а не выдаём константу за прочитанный факт.
def workspace_spdx(name):
    pat = re.compile(r'^name\s*=\s*"%s"\s*$' % re.escape(name), re.M)
    roots = [os.path.dirname(os.path.abspath(LOCK))]
    if os.path.isdir(GITROOT):
        roots += sorted(os.path.join(GITROOT, d) for d in os.listdir(GITROOT))
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [x for x in dirnames if x not in ('target', '.git')]
            if 'Cargo.toml' not in filenames:
                continue
            s = open(os.path.join(dirpath, 'Cargo.toml'), encoding='utf-8',
                     errors='replace').read()
            if not pat.search(s):
                continue
            m = re.search(r'^license\s*=\s*"([^"]+)"', s, re.M)
            return m.group(1).replace('/', ' OR ') if m else None
    return None

lock = open(LOCK, encoding='utf-8').read()
pkgs = re.findall(r'\[\[package\]\]\nname = "([^"]+)"\nversion = "([^"]+)"', lock)
declared = lock.count('[[package]]')
if not pkgs or len(pkgs) != declared:
    sys.exit(f'Cargo.lock: распознано {len(pkgs)} из {declared} пакетов в {LOCK} — '
             'формат изменился, notices не перезаписаны')

def spdx(d):
    s = open(os.path.join(d, 'Cargo.toml'), encoding='utf-8', errors='replace').read()
    m = re.search(r'^license\s*=\s*"([^"]+)"', s, re.M)
    if m:
        return m.group(1).replace('/', ' OR ')
    m = re.search(r'^license-file\s*=\s*"([^"]+)"', s, re.M)
    return 'see license file: ' + m.group(1) if m else 'unstated'

inventory = []
texts = defaultdict(list)
nofile = []
unverified = []

for n, v in sorted(pkgs):
    d = crate_dir(n, v)
    if d is None:
        if n not in WORKSPACE_CRATES:
            sys.exit(f'крейт {n} {v} отсутствует в реестрах {regs} — выполните cargo fetch --locked')
        real = workspace_spdx(n)
        if real is None:
            unverified.append(n)
            note = '  (wgpu / wgpu-native workspace crate; asserted, manifest not read)'
            inventory.append((n, v, WORKSPACE_SPDX + note))
        else:
            note = '  (wgpu / wgpu-native workspace crate)'
            inventory.append((n, v, check_spdx(n, v, real) + note))
        continue
    lic = check_spdx(n, v, spdx(d))
    inventory.append((n, v, lic))
    fs = [f for f in sorted(os.listdir(d))
          if re.match(r'(?i)^(licen[cs]e|copying|notice|unlicense)', f)
          and os.path.isfile(os.path.join(d, f))]
    if not fs:
        nofile.append((n, v, lic))
        continue
    for f in fs:
        t = open(os.path.join(d, f), encoding='utf-8', errors='replace').read().strip()
        texts[hashlib.sha1(t.encode()).hexdigest()].append((n, v, f, t))

if not texts:
    sys.exit('ни одного текста лицензии не собрано — notices не перезаписаны')

RULE = '-' * 79
TMP = OUT + '.tmp'

# Запись через временный файл: прежний OUT усекался первым же open('w'), и обрыв на середине
# (диск, Ctrl-C) оставлял юридический артефакт обрезанным.
def drop_tmp():
    if os.path.exists(TMP):
        os.remove(TMP)

atexit.register(drop_tmp)
w = open(TMP, 'w', encoding='utf-8', newline='\n')
w.write(f"""THIRD-PARTY NOTICES — Rust crates linked into wgpu-native
========================================================

The WebGPU backend (wgpu-native) is a Rust library. On desktop it ships as the
prebuilt libwgpu_native shared library; on iOS and Android it is compiled from
source and statically linked. Either way the crates below end up inside a
like-nes binary, so their notices travel with it.

This file is generated from the Cargo.lock of the pinned wgpu-native revision
({len(pkgs)} crates), reading each crate's own license files out of the local cargo
registry. It complements THIRD-PARTY-NOTICES.txt, which covers the C and C++
components; see THIRD-PARTY.md for the inventory and for how to regenerate.

Where a crate offers a choice of arms, like-nes elects the permissive arm
matching the project license (MIT where MIT is offered). The upstream texts of
all arms a crate ships are reproduced below regardless.


PART 1 — CRATE INVENTORY
{RULE}

""")
for n, v, lic in inventory:
    w.write(f'{n} {v}\n    {lic}\n')

w.write(f"""

PART 2 — CRATES THAT SHIP NO LICENSE FILE
{RULE}

The crates below declare a license in their manifest but ship no license text
in the published crate. The declared identifier is authoritative; the full text
of the common ones is reproduced in part 3, in LICENSE-MIT and in LICENSE-APACHE.

""")
for n, v, lic in nofile:
    w.write(f'{n} {v}\n    {lic}\n')

w.write(f"""

PART 3 — LICENSE TEXTS
{RULE}

Each text below is reproduced verbatim from the crates listed above it.

""")
for h, entries in sorted(texts.items(), key=lambda kv: (-len(kv[1]), kv[1][0][0])):
    w.write('\n' + RULE + '\n')
    for n, v, f, _ in entries:
        w.write(f'{n} {v} ({f})\n')
    w.write(RULE + '\n\n')
    w.write(entries[0][3] + '\n')
w.close()
os.replace(TMP, OUT)
print('crates', len(pkgs), 'unique texts', len(texts), 'no-file', len(nofile))
if unverified:
    print('unverified (asserted, manifest not read):', ' '.join(sorted(unverified)))
print('lines', sum(1 for _ in open(OUT, encoding='utf-8')))
