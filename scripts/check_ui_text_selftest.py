"""Фикстуры #26: плохие/хорошие экраны и настоящий CLI с путями Unicode/пробелы."""
import subprocess
import sys
import tempfile
import io
from pathlib import Path
from unittest.mock import patch

from ui_text_rules import file_hits

KEYS = {"menu.start", "grid.name", "grid.erase", "grid.done"}
GOOD = (
    '#include "ui_context.hpp"\nui.button(ui_id(0, "machine-id"), "menu.start", rect);',
    'ui.slider(id, "menu.start", rect, value, 0, 1, .1f);',
    'ui.field(id, "menu.start", rect, buffer);',
    'ui.glyph_grid(id, "grid.name", buffer, 8, rect, 7, UiGridLabels{"grid.erase", "grid.done"});',
    'UiListItem{"machine-id", "menu.start", true};',
    'auto title = strings.find("menu.start", lang);',
    '// draw_text("STOP");\n/* "STOP" */ ui.button(id, "menu.start", rect);',
    'std::fprintf(stderr, "error: %s\\n", path); static_assert(sizeof(int) >= 2, "int width");',
    'char buffer[16] = ""; buffer[0] = \'\\0\'; int n = 1\'000;',
    'printf(R"tag(literal /*not a comment*/ and ")tag");',
    'ui.button(choose(1, 2), /*key*/ "menu.start", Rect{0, 0, 40, 20});',
    'int Н = 1; ui.button(Н, "menu.start", rect);',
    '%:include "ui_context.hpp"\nui.button(id, "menu.start", rect);',
    'printf(R"tag(line one\nline two)tag");',
)
BAD = (
    'ui.button(id, "START", rect);',
    'ui.button(id, "missing.key", rect);',
    'const auto label = "START"; ui.button(id, label, rect);',
    'draw_text("START");',
    'draw_text("menu.start");',
    'draw_text(R"tag(START)tag");',
    'draw_text(u8"START");',
    'draw_text(L"START");',
    'draw_text(U\'A\');',
    'const char c = \'A\';',
    'ui.button(id, "menu." "start", rect);',
    'ui.button(id, "menu.start" "INJECT", rect);',
    'ui.button(id, "menu.start", wrapper("PLAYER TEXT"));',
    'ui.button(id, unknown("menu.start"), rect);',
    'UiListItem{"machine-id", "START"};',
    'UiGridLabels{"grid.erase", "Done"};',
    'ui.glyph_grid(id, "0123", buffer, 8, rect);',
    '#define TITLE "START"\ndraw_text(TITLE);',
    'fprintf(stderr, "%d", draw_text("START"));',
    'printf("data"); draw_text("START");',
    'draw_text("// ui-text: allow visible text here");',
    'ui.button("wrong-slot", "menu.start", rect);',
    'draw_text("START\\\nGAME");',
    'draw_text(R"(line one\nline two)");',
    'auto message = R"tag(\n/* comment */ "Start"\n)tag";',
    'Нprintf("START");',
    'Нui_id(0,"START");',
    'printf\u0301("START");',
    'custom::printf("START");',
    'object.printf("START");',
    'ptr->printf("START");',
    'custom::std::printf("START");',
    '#define printf draw_text\nvoid screen(){printf("START");}',
    '#define ui_id(a,b) (b)\nvoid screen(){draw_text(ui_id(0,"START"));}',
    '#def\\\nine printf draw_text\nvoid screen(){printf("START");}',
    '#define printf draw_text\nvoid screen(){pri\\\nntf("START");}',
    '%:define printf draw_text\nvoid screen(){printf("START");}',
    '%:define ui_id(a,b) (b)\nvoid screen(){draw_text(ui_id(0,"START"));}',
    '%:def\\\nine printf draw_text\nvoid screen(){printf("START");}',
    'ui.bu\\\ntton(id, "menu.start", rect);',
    'printf(R"( )\\\n"); /*\n)");\ndraw_text("START");',
    'printf(R"(a\\\nb)");',
)


def selftest():
    fails, controls = 0, 0

    def check(ok, name):
        nonlocal fails, controls
        controls += 1
        if not ok:
            fails += 1
            print(f"ui-text-selftest: FAIL {name}")

    for code in GOOD:
        check(not file_hits(code, KEYS), "good " + repr(code))
    for code in BAD:
        check(bool(file_hits(code, KEYS)), "bad " + repr(code))
    hits = file_hits('// "ignore"\n\nui.button(id, "START", rect);', KEYS)
    check(len(hits) == 1 and hits[0][0] == 3, "exact source line survives ignored comments")
    multiline = file_hits('/* first\nsecond */\ndraw_text(R"tag(a\nb)tag");', KEYS)
    check(len(multiline) == 1 and multiline[0][0] == 3, "raw multiline literal reports opening line")
    continued = file_hits('void screen(){\ndraw_te\\\nxt("START");}', KEYS)
    check(len(continued) == 1 and continued[0][0] == 2, "splicing reports the original forbidden backslash line")

    from check_ui_text import MAX_TSV_BYTES, audit, read_keys
    requests = []

    class Input(io.BytesIO):
        def read(self, size=-1):
            requests.append(size)
            return super().read(size)

    with patch.object(Path, "open", return_value=Input(b"key\ten\tru\nmenu.start\tSTART\tSTART\n")):
        loaded = read_keys([Path("changing-size.tsv")])
    check(loaded == {"menu.start"} and requests and all(0 < size <= MAX_TSV_BYTES + 1 for size in requests),
          "TSV read is capped even when file size is not trusted")
    with tempfile.TemporaryDirectory(prefix="ui-text-") as temporary:
        root = Path(temporary) / "тест экранов"
        root.mkdir()
        table, screen = root / "strings.tsv", root / "screen.cpp"
        table.write_text("key\ten\tru\nmenu.start\tSTART\tСТАРТ\n", encoding="utf-8")
        screen.write_text('ui.button(id, "menu.start", rect);\n', encoding="utf-8")
        check(not audit([table], [screen]), "actual file scan accepts known key")
        command = [sys.executable, str(Path(__file__).with_name("check_ui_text.py")),
                   "--strings", str(table), "--screens", str(screen)]

        def run(want, verdict, args=command):
            result = subprocess.run(args, capture_output=True, encoding="utf-8", check=False)
            check((result.returncode == 0) == want and verdict in result.stdout, "CLI " + verdict)

        run(True, "ui-text: PASS 1 screen(s)")
        screen.write_text('ui.button(id, "START", rect);\n', encoding="utf-8")
        run(False, "screen.cpp:1:")
        run(False, "ui-text: FAIL")
        screen.write_text('ui.button(id, "menu.start", rect);\n', encoding="utf-8")
        for bad in ("key\ten\tru\n", "key\tru\ten\n", "key\ten\tru\nmenu.start\t\tSTART\n",
                    "key\ten\tru\nbad key\tA\tA\n", "key\ten\tru\nmenu.start\tA\tA\nmenu.start\tB\tB\n",
                    "key\ten\tru\nmenu.start\tSTART\0\tA\n",
                    "key\ten\tru\n" + "".join(f"key{i}\tA\tA\n" for i in range(4097)),
                    "key\ten\tru\nmenu.start\t" + "A" * (16 << 20) + "\tA\n"):
            table.write_text(bad, encoding="utf-8")
            run(False, "ui-text: FAIL")
        table.write_text("key\ten\tru\nmenu.start\tSTART\t\n", encoding="utf-8")
        run(True, "ui-text: PASS")
        table.write_bytes(b"key\ten\tru\r\nmenu.start\tSTART\tSTART\r\n")
        run(True, "ui-text: PASS")
        table.write_bytes(b"key\ten\tru\n\nmenu.start\tSTART\tSTART\n\n")
        run(True, "ui-text: PASS")
        screen.write_text('\ufeff#include "ui_context.hpp"\n', encoding="utf-8")
        run(False, "UTF8 BOM is forbidden")
        screen.unlink()
        run(False, "ui-text: FAIL")
        run(False, "ui-text: FAIL", command[:2])
        run(False, "ui-text: FAIL", command[:2] + ["--screens", str(table), "--strings", str(table)])
    print(f"ui-text-selftest: {'FAIL' if fails else 'PASS'} {controls} controls (literals, known keys, CLI and empty-scan refusal)")
    return int(bool(fails))
