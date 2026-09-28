#!/usr/bin/env python3
"""Вердикт гейта iOS-симулятора по строкам `[touch]` из системного лога приложения.

    python3 scripts/ios_touch_verdict.py <touch.log> <run>   # вердикт одного прогона
    python3 scripts/ios_touch_verdict.py --selftest          # правила на сломанных фикстурах

XCUITest проигрывает жест целиком и середину не видит, поэтому судится ИСТОРИЯ: строка на каждое
касание с состоянием стика и огня после него (`view.mm`, флаг `--touch-probe`). Точная
последовательность не утверждается: синтезатор XCUIAutomation вставляет лишний короткий тап того
же пальца, и гейт, прибитый к порядку событий, падал бы на артефакте инструмента, а не движка.

Судятся только строки с нонсом ЭТОГО прогона (`run=`): граница выборки лога — время, а время хоста
и симулятора может разойтись на часы, и строки прошлого зелёного прогона дописали бы мутанту
недостающего второго пальца.

Правила доставки (один палец, общий id, никто не держит оба) прерывают разбор: у каждого мутанта
живой самопроверки (`ios_sim_gate_live_selftest.sh`) своя причина, и она обязана быть названа
своей, а не следствием соседней. Правила маршрутизации после них независимы и копятся.
"""

import re
import sys
from pathlib import Path

import py_utf8

LINE = re.compile(r"\[touch\] (down|move|up) id=(-?\d+) id32=(-?\d+) x=\S+ y=\S+ "
                  r"stick=(-?\d+) fire=(-?\d+) run=(\S+)")

NO_LINES = "no [touch] lines"
ONE_TOUCH = "only one touch reached the view"
SHARED_ID = "two touches share one id"
NEVER_BOTH = "stick and fire were never held at once"
NO_MOVE = "the stick touch never moved"
FIRE_DROPS_STICK = "releasing fire released the stick"
STILL_HELD = "a touch is still held after the gesture"


def parse(text, run):
    out = []
    for m in LINE.finditer(text):
        phase, tid, t32, stick, fire, tag = m.groups()
        if tag == run:
            out.append((phase, int(tid), int(t32), int(stick), int(fire)))
    return out


def reused_while_held(ev):
    """Id, который пришёл `down` ещё удерживаемым: адрес UITouch после `up` переиспользуется, и
    повторный `down` отпущенного касания общим id не является."""
    held = set()
    for phase, tid, *_ in ev:
        if phase == "down":
            if tid in held:
                return tid
            held.add(tid)
        elif phase == "up":
            held.discard(tid)
    return None


def verdict(text, run):
    """(находки, сводка): пустой список находок — прогон зелёный."""
    ev = parse(text, run)
    if not ev:
        return [f"{NO_LINES} for run {run}: the app did not run with --touch-probe "
                "or the log was not collected"], ""
    shared = reused_while_held(ev)
    if shared is not None:
        return [f"{SHARED_ID} ({shared}): the second finger is indistinguishable from the first"], ""
    if len({e[1] for e in ev if e[0] == "down"}) < 2:
        return [f"{ONE_TOUCH}: the second finger was never delivered (multipleTouchEnabled?)"], ""
    both = [e for e in ev if e[3] != -1 and e[4] != -1 and e[3] != e[4]]
    if not both:
        return [f"{NEVER_BOTH}: routing never gave the stick and the fire button to two touches"], ""
    stick, fire = both[0][3], both[0][4]
    bad = []
    if not any(e[0] == "move" and e[1] == stick for e in ev):
        bad.append(f"{NO_MOVE}: no move event for touch {stick}")
    if any(e[0] == "up" and e[1] == fire and e[3] != stick for e in ev):
        bad.append(f"{FIRE_DROPS_STICK}: after touch {fire} lifted, the stick was no longer {stick}")
    if ev[-1][3] != -1 or ev[-1][4] != -1:
        bad.append(f"{STILL_HELD}: last state stick={ev[-1][3]} fire={ev[-1][4]}")
    ids32 = {e[2] for e in ev if e[1] in (stick, fire)}
    summary = (f"stick={stick} fire={fire} held together; {len(ev)} events; "
               f"int-truncated ids {'distinct' if len(ids32) == 2 else 'COLLIDE'}")
    return bad, summary


def line(phase, tid, stick, fire, run="R"):
    return f"[touch] {phase} id={tid} id32={tid % 1000} x=1 y=2 stick={stick} fire={fire} run={run}"


GOOD = [line("down", 11, 11, -1), line("move", 11, 11, -1), line("down", 22, 11, 22),
        line("up", 22, 11, -1), line("down", 22, 11, 22), line("up", 22, 11, -1),
        line("up", 11, -1, -1)]
STICK_ONLY = [line("down", 11, 11, -1), line("move", 11, 11, -1), line("up", 11, -1, -1)]

FIXTURES = [
    ("пустой лог", [], NO_LINES),
    ("строки только чужого прогона", [e.replace("run=R", "run=OLD") for e in GOOD], NO_LINES),
    ("второй палец не доставлен", STICK_ONLY, ONE_TOUCH),
    ("зелёный прошлый прогон под мутантом", [e.replace("run=R", "run=OLD") for e in GOOD] + STICK_ONLY,
     ONE_TOUCH),
    ("один палец дважды, адрес переиспользован", STICK_ONLY + STICK_ONLY, ONE_TOUCH),
    ("id схлопнут в константу", [line("down", 1, 1, -1), line("move", 1, 1, -1),
                                  line("down", 1, 1, 1), line("up", 1, -1, -1)], SHARED_ID),
    ("огонь не взят", [line("down", 11, 11, -1), line("move", 11, 11, -1),
                       line("down", 22, 11, -1), line("up", 22, 11, -1),
                       line("up", 11, -1, -1)], NEVER_BOTH),
    ("стик не двигался", [e for e in GOOD if " move " not in e], NO_MOVE),
    ("отпускание огня роняет стик", [line("down", 11, 11, -1), line("move", 11, 11, -1),
                                     line("down", 22, 11, 22), line("up", 22, -1, -1),
                                     line("up", 11, -1, -1)], FIRE_DROPS_STICK),
    ("касание висит после жеста", GOOD[:-1], STILL_HELD),
]


def selftest():
    fails = []
    bad, summary = verdict("\n".join(GOOD), "R")
    if bad or "distinct" not in summary:
        fails.append(f"опорный pass отбит: {bad or summary}")
    for name, lines, reason in FIXTURES:
        bad, _ = verdict("\n".join(lines), "R")
        if not bad or not bad[0].startswith(reason):
            fails.append(f"{name}: ждали «{reason}», получили {bad}")
    for f in fails:
        print(f"[ios-touch] selftest FAIL: {f}")
    print(f"[ios-touch] selftest: 1 pass + {len(FIXTURES)} broken fixtures, {len(fails)} failure(s)")
    return 1 if fails else 0


def main(argv):
    py_utf8.enable()
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) != 3:
        print(__doc__)
        return 2
    bad, summary = verdict(Path(argv[1]).read_text(encoding="utf-8", errors="replace"), argv[2])
    for b in bad:
        print(f"[ios-touch] FAIL: {b}")
    if not bad:
        print(f"[ios-touch] ok: {summary}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
