"""Порчи копий признака обхода и механика их прогона для `check_tree_roots.py`.

Отдельным файлом по тому же основанию, что `ascii_output_check_selftest.py` рядом со своим гейтом:
сверка копий и набор сломанных копий — две ответственности, и растут они независимо (третья копия
приехала со швом хешей, четвёртая — со швом файлов, а с находкой ревью у каждой прибавилось по
второму списку). Зовётся только из `check_tree_roots.py --selftest`, гейтом не является.
"""
import os
import shutil
import sys
import tempfile

from check_tree_roots import COPIES, FS, PY, ROOT, SEAM, SH, gate


def selftest():
    bad = 0

    def run(want, name, mutate):
        nonlocal bad
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "scripts"))
        changed = False
        for rel, _ in COPIES:
            text = open(os.path.join(ROOT, rel), encoding="utf-8").read()
            new = mutate(rel, text)
            changed = changed or new != text
            with open(os.path.join(d, rel), "w", encoding="utf-8", newline="\n") as fh:
                fh.write(new)
        if want == "fail" and not changed:
            sys.stderr.write("tree-roots-selftest: БРАК %s: порча ничего не изменила\n" % name)
            bad = 1
            shutil.rmtree(d, True)
            return
        rc = gate(d, quiet=True)
        shutil.rmtree(d, True)
        ok = (want == "pass" and rc == 0) or (want == "fail" and rc != 0)
        if ok:
            print("tree-roots-selftest: OK   %s (%s)" % (name, want))
        else:
            sys.stderr.write("tree-roots-selftest: БРАК %s: ожидали %s, код %d\n"
                             % (name, want, rc))
            bad = 1

    run("pass", "нетронутые копии совпадают", lambda rel, t: t)
    run("fail", "корень пропал из копии в шелле",
        lambda rel, t: t.replace(' docs/examples"', '"') if rel == SH else t)
    run("fail", "разбор не нашёл копию в шелле",
        lambda rel, t: t.replace("ROOTS_CODE=", "ROOTS_SRC=").replace("ROOTS=", "ROOTS_ALL=")
        if rel == SH else t)
    # Вторая половина признака: расширения. Копия шелла записана флагами grep, поэтому и порча у
    # неё своя — вырезать `--include=*.inl` из значения EXT.
    run("fail", "расширение пропало из копии в шелле",
        lambda rel, t: t.replace(" --include=*.inl", "") if rel == SH else t)
    run("fail", "разбор не нашёл расширения в шелле",
        lambda rel, t: t.replace('EXT="--include', 'EXT_ALL="--include') if rel == SH else t)
    # Порчи на КАЖДУЮ python-копию, а не на одну: копия, которую набор не ломает, выпадает из
    # сверки молча — ровно то расхождение, ради которого гейт и заведён.
    for path, label in ((PY, "ASCII-вывода"), (SEAM, "шва хешей"), (FS, "шва файлов")):
        run("fail", "корень пропал из копии %s" % label,
            lambda rel, t, w=path: t.replace(', "docs/examples"', "") if rel == w else t)
        run("fail", "лишний корень в копии %s" % label,
            lambda rel, t, w=path: t.replace('"platform",', '"platform", "deps",')
            if rel == w else t)
        run("fail", "разбор не нашёл копию %s" % label,
            lambda rel, t, w=path: t.replace("ROOTS = (", "ROOTS_ALL = (") if rel == w else t)
        run("fail", "расширение пропало из копии %s" % label,
            lambda rel, t, w=path: t.replace('".inl", ', "") if rel == w else t)
        run("fail", "лишнее расширение в копии %s" % label,
            lambda rel, t, w=path: t.replace('".mm"}', '".mm", ".rs"}') if rel == w else t)
        run("fail", "разбор не нашёл расширения %s" % label,
            lambda rel, t, w=path: t.replace("EXTS = {", "EXTS_ALL = {") if rel == w else t)

    if bad:
        sys.stderr.write("tree-roots-selftest: FAIL\n")
        return 1
    print("tree-roots-selftest: PASS")
    return 0
