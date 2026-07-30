# Owner verification: the gates a runner cannot close

Three gates are still open, and all three need a machine a CI runner is not: a real desktop
session, a real GPU driver, a real gamepad.

| Gate | Spec | Where |
|---|---|---|
| Linux X11 **and** Wayland — editor renders, gizmo moves an object | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 6 | Linux |
| End-to-end: clone → build → editor → edit game code → change visible | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 8 | Linux **and** Windows |
| Live input: pad passport, profile, runtime rebind, unplug mid-session | [#14](../.context/specs/2026-07-26-framework-input.md) 8 | Linux **and** Windows |

Machine setup (packages, compiler, Developer Command Prompt on Windows) is
[`first-run.md`](first-run.md) — do that first.

## 0. The automated half

```sh
bash scripts/owner_check.sh
```

It writes `build/owner-report-<os>.txt`: machine passport (OS, compiler CMake actually used,
session type, Vulkan device, input nodes), the build gate, the workflow linter, every self-contained
test in the tree, and three timed runs of the edit→build→hot-reload loop. Tests that need paths to
plugins or bundles are skipped by name and say so — CI runs those with arguments on all three OS.

Green means only that this machine agrees with the runners. The three gates below are what the
runners never saw.

## 1. Gate 6 — X11 and Wayland (Linux)

GLFW is X11-only by default, so under a Wayland session the default build runs as an XWayland
client — that answers the X11 question a second time, not the Wayland one. Build **two** trees:

```sh
cmake -S . -B build     -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build-way -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINUX_WAYLAND=ON
cmake --build build     --target editor_shell
cmake --build build-way --target editor_shell
```

`-DLINUX_WAYLAND=ON` needs `wayland-protocols`, `libwayland-dev`, `libxkbcommon-dev`.

Run each binary from the matching session (`echo $XDG_SESSION_TYPE` must print `x11` / `wayland`):

```sh
echo "$XDG_SESSION_TYPE"; ./build/editor_shell        # from an X11 session
echo "$XDG_SESSION_TYPE"; ./build-way/editor_shell    # from a Wayland session
```

In each: the Viewport panel renders, clicking an entity in Hierarchy selects it, dragging the gizmo
moves it, the Inspector numbers follow, Undo puts it back. Screenshot each session — the screenshot
is the gate's evidence, so keep the session type visible (terminal in frame, or note it in the
filename: `gate6-x11.png`, `gate6-wayland.png`).

If the viewport stays black: `vulkaninfo --summary | head` — no device means the driver, not the
editor (`mesa-vulkan-drivers` gets you lavapipe).

## 2. Gate 8 of #13 — the end-to-end loop (Linux and Windows)

The editor has no Play/Build buttons yet: spawning and the build loop are their own targets
(`play_spawn_test`, `build_loop_test`), and the mechanism is what `owner_check.sh` already timed on
this machine. What is left is the human end of the chain — an edit you make reaching pixels you see.

1. Clone into a **fresh directory** (the gate covers a clean machine, not your working tree) and
   build per [`first-run.md`](first-run.md).
2. Open the editor, confirm it draws: `./build/editor_shell` (Windows: `build\editor_shell.exe`).
3. Run the sample game and look at it: `./build/game_sidescroller`.
4. Edit one visible constant — the clear colour in `example_ugly_game/draw.cpp`:
   `a.clearValue = WGPUColor{0.02, 0.02, 0.07, 1.0}` → `{0.25, 0.02, 0.05, 1.0}`.
   It is render-side on purpose: the sim hash must stay `0x32a094e89eacf2f2`, and a gameplay
   constant would move it.
5. `cmake --build build --target game_sidescroller` and run it again — the background is now dark
   red. That is the change being visible.
6. `git checkout example_ugly_game/draw.cpp` — the edit is a probe, not a commit.
7. Note the numbers `owner_check.sh` printed for the loop (`best` / `median`) — spec #13 asks for
   them as a fact per OS, and they go into `first-run.md`.

Windows: run every command from a *Developer Command Prompt for VS*, and run the scripts with
`bash scripts/...` — the git-bash shell does not inherit the exec bit from the index.

## 3. Gate 8 of #14 — live input (Linux and Windows)

```sh
cmake --build build --target framework_input_probe
./build/framework_input_probe example_ugly_game/assets/input.txt
```

The probe opens a small window (keyboard and mouse arrive through it — it must have focus) and
drives the native pad backend (XInput / evdev / GameController). Everything it knows it prints.

1. **Passport → profile.** Plug the pad in *while the probe runs*. It must print one
   `pad 0 CONNECTED vid=… pid=… name="…" -> profile '…'` line. Check the profile matches the
   device: an Xbox pad must not come up as `generic`. A pad that is genuinely unlisted *should*
   say `generic` — that is the fallback working, not a failure. Send this line as-is.
2. **Live resolution.** Push the left stick: `move=(…)` follows it and rests at exactly
   `(+0.00,+0.00)` when centred (that is the deadzone). Press the south button and space — `fire:#`
   lights for both.
3. **Rebind with a conflict.** Press `1` to rebind `fire`, then press a key already used by another
   action. The probe refuses and names the owner. Press `F` to take it anyway, and confirm the
   printed binding table shows the previous owner losing that slot.
4. **Persistence.** Press `S`, `Esc`, then start the probe again: the first line must say
   `overlay loaded … N edit(s)`. That is the restart half of gate 4 on real storage. Press `X` then
   `S` to go back to the clean preset.
5. **Unplug mid-session.** Yank the cable / turn the pad off while the probe runs: it prints
   `pad 0 DISCONNECTED -> profile falls back to 'generic'`, keyboard control keeps working, nothing
   sticks held. Plug it back in — the CONNECTED line comes again.
6. **The real game.** `./build/game_sidescroller` with the pad connected: the ship flies on the
   stick, fire works on the south button. The game reads the same preset from the bundle — this is
   the "sample game on presets" half of the gate.

## What to send back

- `build/owner-report-<os>.txt` from each machine.
- `gate6-x11.png` and `gate6-wayland.png`.
- The probe's console output (the CONNECTED line, the conflict, the overlay-loaded line, the
  DISCONNECTED line).
- The loop timings per OS.

That closes gate 6 and 8 of spec #13 and gate 8 of spec #14, which is what ADR
[0013](../.context/decisions/2026-07-27-desktop-dev-parity.md) and
[0014](../.context/decisions/2026-07-28-framework-input.md) are waiting on to move from *Proposed*
to *Accepted*.
