"""Игровые улики после тача: фактическое движение P1 и новый удар без demo.

Дополняют вердикт владения; CPU проверяет правила. Только живой эмулятор и отрисованные
кадры закрывают критерий приёмки в эмуляторе.
"""

from touch_probe_trace import parse


def actor_verdict(text, run, layout):
    try:
        events, _ = parse(text, run, layout.controls)
        epochs = {}
        for event in events:
            if event.fields.get('demo') != '0':
                return ['interactive demo=0 evidence is required']
            epochs.setdefault(event.epoch, []).append(event)
        if not epochs:
            return ['no actor evidence']
        bad = []
        for history in epochs.values():
            stick_down, moved, fire, punched = {}, set(), None, False
            last_tick = -1
            for e in history:
                f = e.fields
                tick = int(f['tick'])
                if tick < last_tick:
                    raise ValueError('actor ticks went backwards')
                last_tick = tick
                if f.get('player') != '1':
                    continue
                x, z = int(f['player_x']), int(f['player_z'])
                jab_tick = int(f['jab_tick'])
                if jab_tick < 0 or jab_tick > tick:
                    raise ValueError('jab tick outside actor history')
                if e.phase == 'move' and e.owners['stick'] == e.tid and e.tid not in stick_down:
                    stick_down[e.tid] = (x, z, tick)
                    moved.discard(e.tid)
                stick = e.owners['stick']
                if stick in stick_down:
                    start_x, start_z, start_tick = stick_down[stick]
                    if tick > start_tick and x - start_x >= 2:
                        moved.add(stick)
                    if e.phase == 'down' and e.owners['a'] == e.tid and stick in moved:
                        fire = (e.tid, stick, tick, jab_tick)
                    if (fire and (e.tid, stick) == fire[:2] and tick > fire[2] and e.phase == 'up' and
                            fire[2] < jab_tick <= tick and jab_tick > fire[3]):
                        punched = True
                if e.phase == 'up':
                    stick_down.pop(e.tid, None)
                    if fire and e.tid in fire[:2]:
                        fire = None
            if not punched:
                bad.append(f'rotation {history[0].safe.rotation}: no moved P1 followed by an actual jab')
        return bad
    except (KeyError, ValueError, OverflowError) as error:
        return [f'invalid actor evidence: {error}']


def actor_fixture(lines):
    """Независимая фикстура fake-транспорта; не служит уликой живой приёмки."""
    out, tick = [], 0
    for line in lines:
        if '[touch]' in line:
            fields = 'player=1 demo=0 player_z=264'
            x = 200 if ' down id=11 ' in line or ' move ' in line else 236
            pose = 'banderas/jab' if ' up id=22 ' in line else 'banderas/idle'
            jab_tick = 21 if ' up id=22 ' in line or ' up id=11 ' in line else 0
            line += f' {fields} player_x={x} tick={tick} jab_tick={jab_tick} pose={pose}'
            tick += 10
        out.append(line)
    return out


def selftest():
    from touch_probe_layout import parse_layout
    from touch_verdict_selftest import gesture, LAYOUT
    layout = parse_layout(LAYOUT)
    good = '\n'.join(actor_fixture(gesture(1)) + actor_fixture(gesture(3)))
    cases = [('', 'no actor'), (good.replace('demo=0', 'demo=1'), 'demo=0'),
             (good.replace('player_x=236', 'player_x=200'), 'actual jab'),
             (good.replace('jab_tick=21', 'jab_tick=0'), 'actual jab'),
             (good.replace('player=1', 'player=0'), 'actual jab'),
             (good.replace('tick=30', 'tick=0'), 'backwards'),
             (good.replace('player_x=236', 'player_x=nan'), 'invalid actor'),
             (good.replace('tick=30', ''), 'invalid actor'),
             (good.replace('jab_tick=21', 'jab_tick=1'), 'actual jab'),
             (good.replace('run=R', 'run=OTHER'), 'no actor')]
    assert not actor_verdict(good, 'R', layout)
    assert not actor_verdict(good.replace('banderas/jab', 'banderas/idle'), 'R', layout)
    premoved = good.replace('player_x=200 tick=10', 'player_x=236 tick=10')
    cases.append((premoved, 'actual jab'))
    for text, reason in cases:
        bad = actor_verdict(text, 'R', layout)
        assert bad and any(reason in item for item in bad), (reason, bad)
    print(f'rumble-touch-verdict: PASS 1 good + {len(cases)} broken actor histories')


if __name__ == '__main__':
    selftest()
