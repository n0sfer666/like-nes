"""Judge ownership transitions, independently of a platform's gesture synthesizer."""

import math

NO_LINES = 'no [touch] lines'
ONE_TOUCH = 'only one touch reached the view'
SHARED_ID = 'two touches share one id'
NEVER_BOTH = 'stick and fire were never held at once'
NO_MOVE = 'the stick touch never moved'
FIRE_DROPS_STICK = 'releasing fire released the stick'
STILL_HELD = 'a touch is still held after the gesture'


def judge(events, layout):
    if not events:
        return [NO_LINES]
    held = set()
    for event in events:
        if event.phase == 'down':
            if event.tid in held:
                return [SHARED_ID]
            held.add(event.tid)
        elif event.phase == 'up':
            held.discard(event.tid)
    if len({e.tid for e in events if e.phase == 'down'}) < 2:
        return [ONE_TOUCH]
    pairs = {(e.owners['stick'], e.owners['a']) for e in events
             if e.owners['stick'] != -1 and e.owners['a'] != -1
             and e.owners['stick'] != e.owners['a']}
    if not pairs:
        return [NEVER_BOTH]

    bad, active, previous = [], {}, {key: -1 for key in layout.controls}
    moved, released, lifetimes = set(), set(), set()
    for generation, e in enumerate(events):
        lifting = active.get(e.tid)
        if e.phase == 'down':
            active[e.tid] = (e, generation)
        elif e.tid not in active:
            bad.append('move/up without an active down')
        if e.phase == 'up':
            active.pop(e.tid, None)
        owned = [tid for tid in e.owners.values() if tid != -1]
        if len(set(owned)) != len(owned):
            bad.append('one touch owns multiple controls')
        if any(tid not in active for tid in owned):
            bad.append('a control owns an inactive touch')
        stick, fire = e.owners['stick'], e.owners['a']
        if stick in active and fire in active and stick != fire:
            lifetimes.add(((stick, active[stick][1]), (fire, active[fire][1])))
        for key, tid in e.owners.items():
            if tid == -1 or previous[key] == tid:
                continue
            if e.phase != 'down' or tid != e.tid:
                bad.append('control ownership changed without its down')
            if key == 'stick':
                if e.x >= e.safe.width / 2:
                    bad.append('stick down outside the left half')
            elif not layout.buttons[key].contains(e.safe, e.x, e.y):
                bad.append(f'button {key} down outside its layout circle')
        if e.phase == 'move' and e.owners['stick'] == e.tid and e.tid in active:
            down, born = active[e.tid]
            if math.hypot(e.x - down.x, e.y - down.y) > 1 and math.dist(e.knob, down.knob) > 1:
                moved.add((e.tid, born))
        for stick_token, fire_token in lifetimes:
            stick, fire = stick_token[0], fire_token[0]
            if (e.phase == 'up' and e.tid == fire and lifting and lifting[1] == fire_token[1]
                    and stick in active and active[stick][1] == stick_token[1]):
                if e.owners['stick'] != stick:
                    bad.append(FIRE_DROPS_STICK)
                elif previous['a'] == fire and e.owners['a'] == -1:
                    released.add((stick_token, fire_token))
        previous = e.owners
    if not any(stick in moved for stick, _ in lifetimes):
        bad.append(NO_MOVE)
    if not released and FIRE_DROPS_STICK not in bad:
        bad.append('fire was never released independently while the stick was held')
    if moved and released and not any(stick in moved for stick, _ in released):
        bad.append('no single touch pair proves movement and independent fire release')
    if active or any(tid != -1 for tid in events[-1].owners.values()):
        bad.append(STILL_HELD)
    return list(dict.fromkeys(bad))
