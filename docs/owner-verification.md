# Owner verification: the gates a runner cannot close

Five of the six gates below are **closed** — each of those carries the run that closed it, with
the evidence; one is **open**. They stay here as the procedure, because each one needs a
machine a CI runner is not: a real desktop session, a real GPU driver, a real gamepad. A gate is
re-run when a commit touches what it covers; the right-hand column names that surface.

| Gate | Spec | Where | Closed | Re-run when a commit touches |
|---|---|---|---|---|
| Linux X11 **and** Wayland — editor renders, gizmo moves an object | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 6 | Linux | 2026-08-05 | window backend, surface glue, `LINUX_WAYLAND` |
| End-to-end: clone → build → editor → edit game code → change visible | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 8 | Linux **and** Windows | 2026-08-05/06 | build loop, watcher, hot-reload, `win-dev.bat` |
| Live input: pad passport, profile, runtime rebind, unplug mid-session | [#14](../.context/specs/2026-07-26-framework-input.md) 8 | Linux **and** Windows | 2026-08-07 | `engine/input` backends, presets, profiles |
| Physics frame cost against a real frame budget | [#15](../.context/specs/2026-07-26-physics-core.md) 8 | Linux **and** Windows | 2026-08-22 | `engine/framework/physics`, load scenes, solver iterations |
| A target-size level costs a small one's tick, and that tick fits a frame | [#16](../.context/specs/2026-07-26-character-tilemap.md) 7 | Linux **and** Windows | **open** | `engine/framework/character`, `engine/framework/tilemap`, query window |
| The platformer sample plays: slope, one-way, moving platform, and it feels responsive | [#16](../.context/specs/2026-07-26-character-tilemap.md) 8 | **all three** | 2026-08-30 | `engine/framework/character`, `engine/framework/tilemap`, `example_ugly_game/platformer_*` |

The open gate is the character tick cost. It and the physics gate are the two here whose answer a
runner could *print* but not *judge*: both ask whether a number fits a real frame budget on the
slowest machine you own. The right-hand column is the whole point of keeping the procedure: a closed
gate protects nothing if the code under it moves and nobody re-runs it — and the platformer gate,
closed 2026-08-30, sits directly under the module this round keeps changing.

Machine setup (packages, compiler, the right Windows command prompt) is
[`first-run.md`](first-run.md) — do that first. [`owner-setup.txt`](owner-setup.txt) is the same
thing as a copy-paste sheet for a freshly installed Windows, Fedora/Nobara or Arch box.

## 0. The automated half

```sh
bash scripts/owner_check.sh
```

On Windows, `scripts\win-dev.bat check` does the whole thing from any shell — it sets up the x64
environment itself and hands the script to git-bash. The paragraph below is what it automates, and
why it exists.

**On Windows the shell is the whole question.** The script needs two things at once: `cl.exe`,
which only a developer prompt puts on `PATH`, and a POSIX shell, which `cmd` is not. Of the
developer prompts take the one named *x64 Native Tools Command Prompt for VS*: the plain *Developer
Command Prompt* and *Developer PowerShell* default to the 32-bit toolchain (`bin\Hostx86\x86\cl.exe`
in the paths), and this tree is 64-bit only — configure stops on a guard in the root
`CMakeLists.txt` saying so. Git for Windows ships the POSIX shell, so call it from inside that
prompt — it inherits the vcvars environment:

```bat
cd path\to\like-nes
"C:\Program Files\Git\bin\bash.exe" scripts/owner_check.sh
```

Starting from Git Bash instead does *not* work: that shell never ran `vcvars64.bat`, CMake finds no
compiler, and the build gate fails for a reason that has nothing to do with this machine.

Python is the other Windows trap: `python3` there is usually the Microsoft Store stub, which opens
the store and exits without running anything. The script tries `python3`, `python` and `py` and
takes the first that actually executes — the passport line prints which one, or `НЕ НАЙДЕН`, and a
missing interpreter fails the linter stage loudly instead of skipping it. Install Python from
python.org and reopen the shell if you see that.

**Defender is the third Windows trap, and it does not look like one.** Confirmed on the owner's
machine, 2026-08-12: real-time protection refused to execute freshly built, unsigned binaries —
"не удалось проверить подлинность издателя" — and seven of the eight test targets never ran at
all. The shell returned 126 (found, exec denied), the stage printed the same word `FAIL` it prints
for a target that ran and disagreed, and the report read as eight red tests on Windows. It was
zero: that machine had said nothing about seven of them. The script now prints `BLOCKED` with the
exit code for that case and counts it separately in the verdict, so `FAIL` again means what it
says. Before a Windows run, exclude the tree from real-time scanning — PowerShell as
Administrator, once per machine:

```powershell
Add-MpPreference -ExclusionPath 'C:\path\to\like-nes'
```

126 is exec denied, 127 is not found — the latter usually means a DLL next to the `.exe` went
missing, which is a real finding and not a Defender one.

It writes `build/owner-report-<os>.txt`: machine passport (OS, distro, compiler CMake actually used,
session type, Vulkan device, input nodes), the build gate, the workflow linter, every self-contained
test in the tree, and three timed runs of the edit→build→hot-reload loop. Tests that need paths to
plugins or bundles are skipped by name and say so — CI runs those with arguments on all three OS.

Green means only that this machine agrees with the runners. The six gates below are what the
runners never saw.

**A red build gate here is a finding, not a chore.** The runners are `ubuntu-latest`, and a rolling
desktop distro — Nobara/Fedora especially — carries a newer GCC than they do. New GCC releases add
diagnostics, this tree builds with `-Wall -Wextra -Werror`, and a warning nobody on CI can see stops
the build here. That is exactly the value of running the gate on a second Linux: send the compiler
line and the diagnostic instead of silencing it. To collect **all** of them in one pass rather than
the first one:

```sh
cmake -S . -B build-warn -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIKE_NES_WERROR=OFF
cmake --build build-warn 2>&1 | grep -n 'warning:'
```

Use a separate build directory, as above. A tree configured with `LIKE_NES_WERROR=OFF` would keep
the gate green by not enforcing anything, so `build_check.sh` puts the flag back to `ON` and
rebuilds when it finds it off — pointing it at `build-warn` costs you that rebuild for nothing.

## 1. Gate 6 — X11 and Wayland (Linux)

> **Closed 2026-08-05** on Nobara 44 (Intel UHD 620 / Vulkan): Wayland under GNOME and X11 under i3,
> one run each, both PASS. Kept as the procedure — it is what a new machine or a change to the
> windowing path has to be re-run against.

On a Wayland-first GNOME (Nobara/Fedora) the login-screen gear offers no X11 entry at all, and the
first run comes back FAIL on the passport line alone. That case is walked through step by step in
[`gate6-linux.md`](gate6-linux.md); this section is the gate itself.

GLFW is X11-only by default, so under a Wayland session the default build runs as an XWayland
client — that answers the X11 question a second time, not the Wayland one. Build **two** trees:

```sh
cmake -S . -B build     -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build-way -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINUX_WAYLAND=ON
cmake --build build     --target editor_shell
cmake --build build-way --target editor_shell
```

`-DLINUX_WAYLAND=ON` needs `wayland-protocols`, `libwayland-dev`, `libxkbcommon-dev` — on
Fedora/Nobara `wayland-protocols-devel`, `wayland-devel`, `libxkbcommon-devel`. Missing them stops
*configure* on the hunt for `wayland-scanner`, before a single file is compiled: that is the
package talking, not the tree.

Run each binary from the matching session, in the self-test mode — same process, same window, same
surface, but the scenario runs itself and reports:

```sh
./build/editor_shell     --gate6 gate6-x11.png        # from an X11 session
./build-way/editor_shell --gate6 gate6-wayland.png    # from a Wayland session
```

Each run prints a session passport (`XDG_SESSION_TYPE`, the platform GLFW actually drives the window
with, `DISPLAY`/`WAYLAND_DISPLAY`, the adapter), drives select → gizmo hit-test → edit → Inspector →
Undo through the editor's own commands, pushes 150 frames through this session's swapchain, dumps a
PNG of the live frame and exits `0` only if every check passed. **Send the stdout and the PNG** —
that is the gate's evidence, and the passport is what makes the two runs distinguishable.

The mode fails on its own if the Wayland half is not actually Wayland: a default build under a
Wayland session is an XWayland client, the window opens, frames flow, and nothing about Wayland has
been proven. It says so and names the build to make instead of printing a green line about an
untested protocol.

What the process cannot know about itself, and only you can confirm:

- the window is **visible on screen** (a compositor can render an offscreen frame just fine),
- dragging the gizmo with a **real mouse** moves the object, and the Inspector numbers follow.

Screenshot each session with the window on screen; keep the session type visible (terminal in frame).

**Getting an X11 session on a Wayland-first distro.** Nobara, and any recent Fedora, logs you into
Wayland by default, so the X11 half of this gate needs a session you have to pick by hand: log out
(not reboot), and on the login screen choose the session — the gear next to the *Sign in* button in
GNOME, the list in the bottom-left corner in KDE. `echo $XDG_SESSION_TYPE` after logging in is the
proof. If no X11 entry is offered, install one: `sudo dnf install gnome-session-xsession` (GNOME) or
`plasma-workspace-x11` (KDE). Recent Fedora releases dropped the GNOME-on-X11 session outright — if
neither package exists, any X11 session will do, because the gate is about our binary under X11 and
not about a particular desktop: `sudo dnf install i3` (or `openbox`) adds an entry to the same list.

If the viewport stays black: `vulkaninfo --summary | head` — no device means the driver, not the
editor (`mesa-vulkan-drivers` gets you lavapipe; on Fedora that one package already carries radv,
anv and lavapipe together, so there is no per-vendor package to hunt for).

## 2. Gate 8 of #13 — the end-to-end loop (Linux and Windows)

> **Closed 2026-08-05/06** on commit `0e4294c`: Linux (Nobara 44, GCC) and Windows (MSVC 14.44),
> both PASS, dR=+89.705 against a +4 threshold, sim-golden intact. Kept as the procedure.

The editor has no Play/Build buttons yet: spawning and the build loop are their own targets
(`play_spawn_test`, `build_loop_test`), and the mechanism is what `owner_check.sh` already timed on
this machine. What is left is the chain end to end — an edit reaching pixels — and that whole chain
is one command:

```sh
bash scripts/gate8_e2e.sh
```

It clones this repository into `build/gate8/clone`, builds it from scratch, renders frames, patches
the clear colour in `example_ugly_game/draw.cpp` (`{0.02, 0.02, 0.07}` → `{0.25, 0.02, 0.05}`),
rebuilds, renders again, and then **measures** the two frames instead of asking you to look: the
mean red channel has to rise and to outrun green and blue, because "the frame changed" would also
be true of any render jitter. The sim hash has to stay `0x32a094e89eacf2f2` in the same run — the
constant is render-side on purpose, and a gate that only proved visibility would pass a broken
gameplay build too.

The patch lives inside the clone, so your working tree is never touched and there is no revert step
to forget. The clone is deleted at the end (`GATE8_KEEP=1` keeps it); the report, both frames and
the build logs stay in `build/gate8/`. **Send `build/gate8/gate8-report-<os>.txt`** — it carries the
cloned commit, the two colour readings and the verdict.

The first build downloads the dependencies, so this needs network and takes as long as a cold build
on this machine (`GATE8_FRAMES` shortens the render, not the build). If the clone builds the editor,
the report says so and prints the `--gate6` command for it — that is section 1, run from the fresh
clone.

One number is still yours to record: `owner_check.sh` prints the edit→build→hot-reload loop timing
(`best` / `median`), and spec #13 asks for it as a fact per OS. It goes into `first-run.md`.

Windows: `scripts\win-dev.bat gate8` runs it from any shell — same wrapper as `check`, and for the
same reason. This gate needs vcvars and a POSIX shell *at once*: it clones the tree and builds the
clone from scratch, so starting from Git Bash alone stops at "no compiler". By hand it is an *x64
Native Tools Command Prompt for VS* plus `"C:\Program Files\Git\bin\bash.exe" scripts/gate8_e2e.sh`
— the exec bit does not survive the index there, so the interpreter is always named explicitly.

## 3. Gate 8 of #14 — live input (Linux and Windows)

> **Closed 2026-08-07** on Linux (Nobara 44, evdev, pad passport `vid=045e pid=0b12`) and Windows
> (MSVC, XInput, `vid=045e pid=02ff`): both resolved the Xbox profile, all four stick directions
> reported `yes` with the contract's signs over the full `[-1.00,+1.00]` range, the rebind conflict
> was refused by name (`source already bound to 'jump' slot 0`) and taken by `F`, the overlay
> survived a restart, hotplug printed DISCONNECTED/CONNECTED without a stall, and the sample game
> was played to the boss on the pad on both OSes. Kept as the procedure.
>
> **Re-run 2026-08-08** on `ba59cca`, Windows only and step 1 only, because that is the surface
> commit `404b29f` moved: the passport now falls back to the documented `XInputGetCapabilities`
> when the undocumented ordinal 108 does not answer. Same pad, same result — `vid=045e pid=02ff
> -> profile 'Microsoft Xbox' (deadzone 0.18, trigger 0.12)`, `cold-start scan` correctly reported
> no pad before it was plugged in, and the two directions pushed carried the contract's signs
> (`right raw lx=+0.54 -> move x=+0.44`, `up raw ly=-0.54 -> move y=+0.44`). An unchanged passport
> is the pass here: the fallback was added so the line keeps printing, not to change what it says.
>
> Two findings the live run produced, both fixed here: the witness first judged deflection by the
> *resolved* axis, which the keyboard also writes to, and an idle XInput pad rests at `raw
> lx=-0.01`, which an exact compare against zero read as "the preset ate the stick". The mouse is
> live as a *button* only — the sample layout binds `mouse:left → fire` and no `mouseaxis:` row,
> so a trackpad moves nothing there by composition of the manifest, not by a defect.
>
> **Re-run 2026-08-29** on `8f822eb`, Windows only (MSVC 14.44, NVIDIA MX150 / Vulkan), all six
> steps, PASS. Same pad and the same answer as the closing run — `vid=045e pid=02ff -> profile
> 'Microsoft Xbox' (deadzone 0.18, trigger 0.12)`. The rebind conflict was refused by name and taken
> by `F`, the overlay survived the restart (`overlay loaded ... 2 edit(s)`) and was cleaned back down
> to `bye - overlay empty`, hotplug fell back to `generic` and came back, and the sample game flew on
> the stick under `gamepad: XInput (Windows)` with a clean exit. Both branches of `cold-start scan`
> fell out of this one sitting: `NO pad on any of 8 slots` when the pad was plugged in mid-run, and
> `1 pad(s) already connected` on the next start.
>
> Two things this run does *not* assert. The four directions came from two probe sessions rather than
> one `axis report` — `up ly=-0.68 -> (+0.00,+0.61)` and `left lx=-0.61 -> (-0.52,+0.00)` in the
> first, `right lx=+0.59 -> (+0.51,+0.00)` and `down ly=+0.50 -> (+0.00,-0.39)` in the second — so
> each sign is verified against the expectation printed beside it, but not the four of them agreeing
> at once. And step 3's refusal was reachable only because the run started from a clean overlay: the
> first attempt did not. The file the 2026-08-07 closure left behind was still on disk three weeks
> later, holding `fire -> key:j` with `jump` slot 0 stripped, and it is why the fourth trap below is
> written down at all.

```sh
cmake --build build --target framework_input_probe
./build/framework_input_probe engine/framework/input/probe_input.txt
```

Run it from the repo root: the manifest path is relative, and the manifest is the *source* preset —
the probe bakes it in-process, so there is no separate bake step. It is the probe's own preset, not
the game's: the game has exactly one action (`fire`), and `find_conflict` skips the action being
rebound, so step 3 below is unprovable on the game's manifest — it always looks passed. The game
manifest could not simply grow a second action either: a golden `bundle_hash` is pinned on it. On
Windows use
`scripts\win-dev.bat probe`: `cmake` is not on PATH there until vcvars has run, which is the same
reason `check` and `gate8` have wrappers. It rebuilds that one target (a pull leaves the old binary
in place silently) and runs it from the tree root whatever shell you started in.

The probe opens a small window (keyboard and mouse arrive through it — it must have focus) and
drives the native pad backend (XInput / evdev / GameController). Everything it knows it prints.

Three traps before you start, all of which make a working pad look like a broken backend. The
probe's `cold-start scan: …` line — printed on the first tick, right after the bindings — tells the
two halves apart: it says out loud whether the backend reported any pad on the very first poll, so "the OS never handed it over" and "we lost the
event" stop looking alike.

- **Permissions (Linux).** The evdev backend reads `/dev/input/event*`; a desktop session normally
  gets them through logind's `uaccess`, but a pad seen by `ls /dev/input` and not by the probe means
  exactly that seam. `sudo usermod -aG input $USER`, then log out and back in.
- **Steam (both).** On a gaming distro (Nobara ships Steam) a running Steam with Steam Input on
  re-presents the pad as a *virtual* Xbox 360 controller and hides the real one. The passport line
  then names the wrong device through no fault of ours. Quit Steam completely for this gate — the
  point is what the OS reports about the physical pad.
- **Xbox Game Bar (Windows).** Confirmed on the owner's machine, 2026-08-06: a pad plugged in since
  boot was invisible to XInput for the whole session and became visible the moment the cable was
  re-seated. Nothing on our side polls too rarely to catch it — every slot is polled every frame, so
  a lost connect event would heal by itself on the next one; XInput simply reported no device.
  Settings → Gaming → Xbox Game Bar → Off (and quit Steam), or re-plug the pad after the probe
  starts.

A fourth trap belongs to the *procedure* rather than the backend, and it is worse than the three
above because it makes steps 3 and 4 look **passed** instead of broken. **The run starts from a
clean overlay.** The rebind overlay outlives the run that wrote it, and the cleanup that removes it
is the last sentence of step 4 — the easiest line in this section to skip once the pad already
works. It was skipped: on 2026-08-29 the file left by the 2026-08-07 closure was still sitting in
`%APPDATA%\like-nes\controls_probe.txt`, three weeks old, holding `fire → key:j` with `jump` slot 0
stripped. Started on top of that file, this gate proves nothing twice over — step 3 asks for a
conflict on a source that **nobody owns any more**, so the probe accepts `J` without a word and the
refusal the step exists to see never appears; step 4 finds `overlay loaded … N edit(s)` printed on
the *first* start, before anything was saved, so its restart half is carried by a file older than
the binary. Both look exactly like a pass.

The probe already prints the answer — second line of the run, right after the resolved move axes.
Read it before step 1, and it must say

```
[probe] no overlay for preset 'probe' at <path> - clean preset
```

`overlay loaded` there instead means the previous run was never cleaned up: quit, delete the file
the line names (the path is *in* the line, so there is nothing to look up per OS), start again. The
`X` then `S` cleanup at the end of step 4 stays where it is — this is the check for the run where
it was forgotten, and a gate whose precondition is only a habit is not a gate.

1. **Passport → profile.** Plug the pad in *while the probe runs*. It must print one
   `pad 0 CONNECTED vid=… pid=… name="…" -> profile '…'` line. Check the profile matches the
   device: an Xbox pad must not come up as `generic`. A pad that is genuinely unlisted *should*
   say `generic` — that is the fallback working, not a failure. Send this line as-is.
2. **Live resolution.** The first line of the run names which axes are being judged:
   `move axes resolved by name: move_x=0 move_y=1 (order comes from the manifest)`. Those indices
   are *data*: moving an `axis` row in the manifest renumbers them. The probe looks both axes up by
   name instead of assuming 0 and 1, and a preset that declares neither — or declares more axes than
   the frame holds — says so and exits rather than judging some other axis under the name `move_x`.
   Send this line too: without it a swapped pair reads exactly like a backend that inverted the
   stick.
   Then push the left stick fully in all four directions, one at a time, and hold
   each for a moment: the probe prints one `stick right/left/up/down: raw … -> move=(…)` line per
   direction the first time it sees it, and on exit an `axis report` block with the extremes of the
   whole session. Then let the stick centre — `move=(…)` must rest at exactly `(+0.00,+0.00)` (that
   is the deadzone). Press the south button and space — `fire:#` lights for both.
   The signs are the point: right must read `x > 0`, and **up must read `y > 0`**. This is not
   pedantry — the three backends disagree on the sign of the raw Y axis (evdev grows downwards,
   XInput and GameController upwards), so an inverted stick is a *per-platform* defect that a pad
   tested on one OS cannot reveal. The engine's contract is in `engine/input/codes.hpp` — +X right,
   +Y **down** for the raw axis — and the preset flips it once with `padaxis:-ly`, which is why
   `move` reads up as positive. Each printed line carries the expected sign next to the measured
   one, so nothing has to be remembered while testing.
   The `move=` figures in the live status line **cannot be sent**: that line is redrawn over itself
   with `\r`, so a pasted log keeps only its last frame. The per-direction lines and the exit report
   exist because of exactly that — they are the evidence that survives copy-paste. The verdict per
   direction is one of four: `yes`, `NO` (never pushed that far), `EATEN` (the stick arrived and the
   preset resolved nothing — binding or deadzone) and `INVERTED` (it resolved with the sign opposite
   to the contract, which is the per-platform defect this step hunts). Note that `move` is fed by
   the keyboard too, so it moving on its own proves nothing about the pad — only the `raw stick`
   figures do, and the report keeps them apart for that reason.
3. **Rebind with a conflict.** Press `1` to rebind `fire`, then press `J` (or `K`) — both belong to
   `jump`, the second action this manifest exists for. Now press **Enter**: the probe must refuse
   and name `jump` as the owner. *Then* press `F` to take it anyway, and confirm the printed binding
   table shows `jump` losing that slot. Two ways this step silently proves nothing: pressing a
   *free* key (the probe accepts it without a word, which looks exactly like a pass), and pressing
   `F` straight away — `F` is the force, it never asks, and the refusal this step is about never
   appears.
4. **Persistence.** Press `S`, `Esc`, then start the probe again: the line right after the resolved
   move axes must say `overlay loaded … N edit(s)`. That is the restart half of gate 4 on real storage. Press `X` then
   `S` to go back to the clean preset.
5. **Unplug mid-session.** Yank the cable / turn the pad off while the probe runs: it prints
   `pad 0 DISCONNECTED -> profile falls back to 'generic'`, keyboard control keeps working, nothing
   sticks held. Plug it back in — the CONNECTED line comes again.
6. **The real game.** The game is a separate target, and step 3 above built only the probe, so it
   has to be asked for by name — `./build/game_sidescroller: No such file` means it was never built,
   not that it is missing from the tree:

   ```sh
   cmake --build build --target game_sidescroller
   ./build/game_sidescroller
   ```

   (Windows: `scripts\win-dev.bat game`.) With the pad connected the ship flies on the stick and
   fire works on the south button. The game reads the same preset from the bundle — this is the
   "sample game on presets" half of the gate. If the stick moves nothing here but did move `move=`
   in the probe, say so: the two read the same axes through different preset tables, and that split
   is the whole diagnosis.

## 4. Gate 8 of #15 — the physics frame cost (Linux and Windows)

> **Closed 2026-08-22** on the Intel UHD 620 box under both OS, at the declared **350 bodies** and
> 16 iterations: `heap` mean 3.560 ms (21.4% of the 16.67 ms frame) on Windows/MSVC and 2.931 ms
> (17.6%) on Nobara/gcc, `allocs=0` on all three scenes. The sweep that settled that body count is
> the subsection below. Kept as the procedure — re-run it when the solver, the load scenes or the
> iteration count move.
>
> **Re-run 2026-08-29** on `64bd866`, Linux only, as the physics stage of `owner_check.sh`: same box,
> same declared count, `heap` mean 2.952 ms (17.7%), `scatter` 0.054 ms, `column` 0.161 ms,
> `allocs=0` on all three, every counter on its pinned reference. Within 0.7% of the closing run.
>
> The Apple M3 Pro figures below (macOS 26.5.2, Apple clang 21.0.0, Release: 3.64 ms mean at **500**
> dynamic bodies) are the older scene, kept because the reasoning about `worst` came out of them.
>
> The number moved from 2.25 ms when `VELOCITY_ITERATIONS` went from 8 to 16 (round of 2026-08-12):
> the solver is the cost of the step, so doubling its iterations costs about what it says. What the
> extra iterations buy is stack depth, and that is the reason the price is paid — see `solver.hpp`
> and `framework_physics_depth_test`. Judge the new number, not the old one.

CI **asserts** this target on all three OS, in Release and in Debug, and as of run
[31393763850](https://github.com/n0sfer666/like-nes/actions/runs/31393763850) it is green there —
the counters came back identical to the unit on ubuntu, macos and windows runners, in both
configurations. That is a separate statement from the assertion, and it stays unwritten until a run
says so: "CI is green" put down ahead of the run is exactly how #12 spent six red runs in a row.
What CI asserts is the **work counters**, not the time. That split is deliberate and explained in
`counters.hpp`: the counters are integer, deterministic and must be identical on all three OS to the
unit, while the wall clock on a shared GitHub runner wanders by a factor. A red step saying "the
frame took 3.1 ms instead of 2" would be switched off within a week, and the whole gate with it.

So the runner answers "does the engine do the same amount of work everywhere" and cannot answer "does
that work fit a frame". The second question is about a target machine, and the only target machines
that exist are yours.

```sh
cmake --build build --target framework_physics_perf_test
./build/framework_physics_perf_test
```

(Windows: `scripts\win-dev.bat check` builds the tree; then
`build\framework_physics_perf_test.exe`.)

Expected output. The counter values are **not repeated here on purpose**: they are pinned as
constants inside the binary (`framework_physics_perf_test.cpp`), and a copy in a runbook is a copy
nothing checks — it goes stale on the first re-pin and then quietly asks you to confirm last month's
numbers. A differing counter prints its own `FAIL: <scene>: <field> = N, reference says M` line and
the run exits non-zero, so what you check by eye is the shape, `allocs=0` and the final verdict. The
`worst=` / `mean=` numbers may legitimately differ between machines; the measured table lives in
`.context/specs/2026-07-26-physics-core.md`.

```
framework physics perf gate (350 bodies)
  heap: bodies=350 pairs=<n> broad=<n> narrow=<n> vel=<n> pos=<n>
  heap: worst=<time> ms mean=<time> ms allocs=0
  scatter: bodies=350 pairs=<n> broad=<n> narrow=<n> vel=<n> pos=<n>
  scatter: worst=<time> ms mean=<time> ms allocs=0
  column: bodies=0 pairs=0 broad=<n(n-1)/2> narrow=0 vel=0 pos=0
  column: worst=<time> ms mean=<time> ms allocs=0
framework-physics-perf: PASS
```

What to judge, in this order:

1. **Any `FAIL:` line** is the serious finding, and it outranks any timing. A counter that misses its
   reference means two OS disagree about the arithmetic — the same class of defect the state golden
   exists to catch. Report it before anything else, with the full output.
2. **`heap: mean=`** against a 16.67 ms frame. At the declared **350 bodies** the sweep of 2026-08-22
   measured 3.560 ms (21.4%) on Windows/MSVC and 2.931 ms (17.6%) on Nobara/gcc. The figures below
   are the older 500-body scene and are kept because the reasoning about `worst` came out of them:
   3.64 ms (22%) on the M3 Pro, 8.583 ms (51%) on the Nobara laptop, 9.594 ms (58%) on Windows on
   that same laptop. Judge
   `mean`, not `worst`, and the 2026-08-13 sweep put numbers on why: across three repeats of one
   cell — same binary, same scene, idle Mac — `mean` landed within **0.8%** (2.253 / 2.254 / 2.270)
   while `worst` moved **20%** (2.411 / 2.406 / 2.894). On the Windows box `worst/mean` runs 1.4–1.8
   against 1.1–1.2 for Linux on that same hardware, so half of a Windows `worst` is the scheduler,
   not the step. `worst` stays in the table as an observed fact about the machine; it is not the
   criterion that sets a body count. A machine where `mean` passes 8 ms (half the frame) is the
   honest ceiling — the 500-body scene passed it on both live machines, and that is why the declared
   count came down to 350 (see below). Judge against the **slowest** machine you have: the M3 Pro figure
   describes the M3 Pro, and the whole point of running this on your hardware is that the fast row
   cannot answer for the engine.
3. **`allocs=0` on all three scenes.** Anything else means the step went to the heap on a scene the
   runner does not exercise this way.

`column` reporting `bodies=0` and zeros across the solver is **correct, not a broken scene**: its
bodies are kinematic on purpose, so nothing is solved. `bodies=0` is not "nothing moves" — the step
still walks all 350 of them through integration and the world-bound clamp; the counter reports the
bodies gravity and damping were applied to, which is the load on the solver (`counters.hpp`). That
scene exists to measure one thing — the broadphase degenerating to the full pairwise scan,
61075 = 350·349/2.

### Closed 2026-08-22: 350 bodies at 16 iterations

The round that raised `VELOCITY_ITERATIONS` from 8 to 16 measured the price on one machine — the M3
Pro, where the heap went 2.25 → 3.64 ms, 22% of the frame. Your sweep of 2026-08-13 measured the
matrix on the Intel UHD 620 box under both OS, and it settles the iteration question and reframes
the body one. Windows/MSVC, `mean` against the 16.67 ms frame:

| cell | Windows mean | Linux mean |
|---|---|---|
| 16 iterations, 500 bodies | 10.865 ms (65%) | 5.962 ms (36%) |
| 8 iterations, 500 bodies | 7.292 ms (44%) | 4.070 ms (24%) |
| 16 iterations, 300 bodies | 3.332 ms (20%) | 2.051 ms (12%) |

**Iterations stay at 16.** Halving them only pays at 500 bodies (−33% on both OS). At 300 bodies it
pays nothing and costs a little: 2.193 ms against 2.051 on Linux, because worse convergence leaves
more contacts standing (`pairs` 830 against 750). Since the body count is what is coming down, the
cheap-looking knob buys nothing on the scene we would actually ship — and it would hand back the
stack depth of 10/11 that 16 iterations were bought for on 2026-08-08.

**Bodies are the expensive axis.** 500 → 300 at 16 iterations is 3.3× cheaper because the broadphase
here is quadratic: `broad` 11951 against 4252, and (500/300)² = 2.78. What is missing is the middle —
nothing between 300 and 500 has been measured, and interpolation is not measurement:

```
bash scripts/perf_sweep.sh 16:400 8:400 16:350 8:300
```

**Four cells, not three, and `8:400` is the reason.** With `16:400 16:350 8:300` every difference
reads two ways at once: 400 bodies would be measured only at 16 iterations and 8 iterations only at
300, so neither axis has a partner holding the other fixed. `8:400` pairs with `16:400` on
iterations and with `8:300` on bodies, and both comparisons become one-variable.

**The answer, measured 2026-08-22** (median of three repeats, `mean` against the 16.67 ms frame):

| cell | Windows/MSVC | Nobara/gcc |
|---|---|---|
| 16 iterations, 400 bodies | 4.595 ms (27.6%) | 3.843 ms (23.1%) |
| 8 iterations, 400 bodies | 3.531 ms (21.2%) | 2.971 ms (17.8%) |
| **16 iterations, 350 bodies** | **3.560 ms (21.4%)** | **2.931 ms (17.6%)** |
| 8 iterations, 300 bodies | 2.672 ms (16.0%) | 2.265 ms (13.6%) |

Both crosses closed and agreed across the two OS: 16 → 8 iterations at 400 bodies is −23% on each,
400 → 350 bodies at 16 iterations is −23% and −24%. The decisive row is the pair that costs the
same: `16:350` against `8:400` is 3.560 vs 3.531 on Windows and 2.931 vs 2.971 on Linux — inside the
noise on both machines. Halving the iterations buys nothing that dropping fifty bodies does not buy,
and 16 iterations were bought for the 10/11 stack depth (decision of 2026-08-08). So the iterations
stay and **350 bodies is the declared count**: 21.4% of the frame on the slowest machine in the set,
against 58% for the five hundred it replaces.

The `ШУМ` mark in that run landed on `8:400` under Windows and needed no rerun. Repeat 2 lifted
`column` along with the heap (0.291 → 0.576 ms) on literally identical work — the signature of a
foreign process — while repeats 1 and 3 agreed to 0.9%. The ratio against `16:400` corroborates the
median from the other side: 1.30 on Windows, 1.29 on Linux. The flag fired on `worst`; the verdict
is taken on the median.

The `8:300` cell is a rerun: **the Windows 8/300 cell of 2026-08-13 was thrown out.** The `column`
scene runs zero solver iterations and, at equal body count, exactly the same work — so its cost in
the two 300-body cells has to match. It did not: 0.214 against 0.511 ms mean (Linux, same cells:
0.149 against 0.154), and all three scenes in that cell swelled together. Those numbers described a
neighbouring process, not the step.

That control is now in the script, and it is no longer the only one. Every cell is measured
`REPEATS` times (3 by default) **without a rebuild in between** — the compiler is what costs minutes
here, the run costs seconds — and the judging number in the table is the **median** of those repeats,
with their spread `(max − min) / median` in a column of its own. That is the direct answer to "can
this row be trusted", where the `column` scene was only ever an indirect one. The run of 2026-08-13
(2) is why: the same matrix on the same box came out 21–24% slower than the previous run in all four
cells at once, while the ratios between cells reproduced to 1%. A row whose repeats spread by 5% or
more is marked `ШУМ` — three repeats of one cell spread by 0.8%, so the threshold sits three times
above jitter that was measured rather than assumed.

The `column` control stayed and gained an absolute floor: a cell is marked only if its `column` mean
exceeds the cheapest cell of its body-count group by more than 10% **and** by more than 100 µs per
frame. The same run of 2026-08-13 (2) is why. A relative threshold without a floor is loudest
exactly where the number is smallest, and it fired on 31 µs per frame — 0.19% of the budget. The
floor sits between jitter that was measured (18 µs across repeats of one cell) and the real Windows
finding (0.214 against 0.511 ms — 297 µs).

**`ШУМ` marks the row; it does not fail the run** — the exit code stays 0. A red run would say "these
numbers are unusable", and that is not true: the decision is made on ratios between cells of one
run, and those survive machine shake. Only the absolute milliseconds of a marked row do not travel —
rerun that row on its own if you need them. A flag that fails runs for nothing gets ignored, and
then it is silent when it matters too. A group with only one cell prints `?`, and so does a run with
`REPEATS=1`: nothing to compare against is not the same as clean.

The rules are checked by fixtures taken from these very runs (`bash scripts/perf_sweep_selftest.sh`,
also a stage in `preflight.sh`), the table and its verdicts included — a control that could only be
proven by a foreign process seizing the machine on cue would otherwise never be proven at all. The
script also waits `SETTLE_S` seconds (10 by default) between the rebuild and the first repeat: the
measurement used to start on a CPU that had just had every core busy compiling that same target.

**On Windows it is two steps, not one**, and for the same reason `owner_check.sh` and
`gate8_e2e.sh` are reached through `win-dev.bat` there: a plain Git Bash window has never seen
`vcvars64.bat`, so `cl.exe` compiles nothing in it. Raise the environment first, then run the sweep
in the shell that inherited it — the second line spells `bash.exe` out in full because Git for
Windows puts only its `cmd\` directory on `PATH`, so bare `bash` may not resolve:

```
scripts\win-dev.bat shell
"%ProgramFiles%\Git\bin\bash.exe" scripts/perf_sweep.sh 16:400 8:400 16:350 8:300
```

Run it from a Git Bash window by mistake and the script stops before it touches a single header,
naming those two lines — the failure it would otherwise produce is twenty lines of ninja output
about a compiler, which sends you fixing the wrong thing.

**Two refusals fire before the first rebuild**, because both of them cost a whole run rather than a
row, and both have already been paid for. A checkout behind `origin` measures with the *old* script —
that is where the run of 2026-08-22 went, and its table came back in a format retired eight commits
earlier. A build directory configured as `Debug` is worse: the numbers come out several times slower
while **every control stays green**, since the cells slow down together, their ratios hold, `column`
matches across the group and the spread stays tight. There is nothing in such a table to tell it
apart from an honest one, which is why it is refused rather than flagged. Each refusal names its own
one-line cure (`git pull --ff-only`, `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`).

Provenance is printed twice — as the first line of the run and again immediately above the table:

```
perf-sweep: инструмент <sha> от <date>[, дерево ГРЯЗНОЕ], ячеек 4, повторов 3
```

Once, in a header a hundred lines of build output away from the table, is once too few: what gets
pasted back is the *tail* of the output, so the version has to travel inside it. The dirty-tree note
does not stop the run — the script exists to be edited while it is used — but a table taken from a
tree that differs from `origin` can be read; it cannot be used to pin numbers.

The script edits both headers, rebuilds only `framework_physics_perf_test`, restores them from HEAD
and rebuilds once more; it refuses to start if either header has uncommitted changes. Red verdicts
*inside* the run are expected and it says so on every cell: the reference counters are pinned to 500
bodies at 16 iterations, so every other cell misses them by construction. The timings are printed
before the verdict and are what the run is for.

Send back the final table. Its columns are `итераций тел медиана %бюдж худшее %бюдж разброс vel
метка`, and the last two are controls rather than results. The `vel` column: two cells that differ
in iterations must differ in `vel`, and the script prints `FAIL` in place of a verdict and exits 1
if they do not, because equal counters mean every row was measured on one binary that never got
rebuilt. A table like that looks flawless, which is exactly the danger. The `метка` column carries
both noise checks — `ok`, `?` (nothing to compare against) or `ШУМ`.

## 5. Gate 7 of #16 — the character tick cost (all three OSes)

> **Open.** The counter half of this gate is closed by CI and needs no machine of yours: the same
> scripted route over a 256×32 map and over a 1024×256 one — thirty-two times the area — returns the
> *same* `queries`, the same `scanned`, the same worst tick and the same trajectory hash, on three
> OSes and in both configurations (`framework_character_perf_test`). That is invariant 4 of spec
> [#16](../.context/specs/2026-07-26-character-tilemap.md) — the cost of a query does not depend on
> the size of the level — and it is observable only by **comparison**, never by an absolute number.
>
> What is left is the same half that keeps the physics gate open one section up: whether the work
> those counters describe fits a frame **on your hardware**. The Apple M3 Pro run of 2026-08-30
> (macOS 26.5.2, Apple clang, Release) put the target-size level at `worst` 0.087 ms and `mean`
> 0.0103 ms — 0.06% of a 16.67 ms frame — but the M3 Pro figure describes the M3 Pro, and the
> machine that decides is the slowest one you have.

This gate is the character half of the frame-cost stage of `owner_check.sh`, so a full report
already carries it. Alone:

```sh
cmake --build build --target framework_character_perf_test
./build/framework_character_perf_test
```

(Windows: `scripts\win-dev.bat check` builds the tree, then
`build\framework_character_perf_test.exe`. Both frame-cost measurements at once, physics and
character, are `bash scripts/owner_perf.sh`.)

Expected output. The counters are **not repeated here**, for the reason given one section up: they
are pinned as constants inside the binary and a copy in a runbook is a copy nothing checks. A
differing counter prints its own `FAIL: <field> = N, reference says M` line and the run exits
non-zero.

```
framework character perf gate (a target-size level costs a small one's tick)
  small:  map=256x32 (8192 tiles) queries=<n> scanned=<n>
  small:  worst tick = <n> queries / <n> tiles, hash=<hash>
  small:  worst=<time> ms mean=<time> ms allocs=0 cols=[<lo>, <hi>]
  small:  ground=<n> air=<n> ceiling=<n> slope=<n>
  target: map=1024x256 (262144 tiles) queries=<n> scanned=<n>
  target: worst tick = <n> queries / <n> tiles, hash=<hash>
  target: worst=<time> ms mean=<time> ms allocs=0 cols=[<lo>, <hi>]
  target: ground=<n> air=<n> ceiling=<n> slope=<n>
framework-character-perf: PASS
```

What to judge, in this order:

1. **Any `FAIL:` line**, ahead of every timing — the same rule and the same class of defect as the
   physics gate: a counter off its reference means two machines disagree about the arithmetic.
2. **The counter lines are identical between `small:` and `target:`** — `queries`, `scanned`, the
   worst tick and the hash. That is the invariant itself, and it is the one thing here you can check
   by eye without knowing what a good number looks like. Only the two `worst=`/`mean=` lines may
   differ between the maps, and they differ by the clock, not by the map.
3. **`target: mean=` added to the physics `heap: mean=`** against the 16.67 ms frame. Both run in
   the same frame of a real game, so their costs add; the character tick is the small summand and is
   expected to stay one — a machine where it reaches a millisecond is the finding, and it is a
   finding about the tick, not about the map, because the map cannot make it grow.
4. **`allocs=0` on both maps.** Anything else means the tick went to the heap on a route the runner
   does not exercise this way.

`ceiling=` in the low tens against `ground=` in the hundreds is **correct, not a thin route**: the
generated pattern carries one ceiling ledge and the scripted run passes under it a few times per
lap. Those four tallies are asserted non-zero on purpose — a route that stopped touching a slope, a
ceiling or the air would go on measuring an easier level and printing PASS, which is exactly the
shape of a gate that has quietly stopped gating.

## 6. Gate 8 of #16 — the platformer sample plays (all three OSes)

> **Closed 2026-08-30** by the owner, who ran the sample and reported the control responsive.
> **The banner names a confirmation, not artefacts:** the screen recording, the startup line and the
> six answers below were not handed over, so unlike the four gates above this one carries no
> evidence anyone else can re-read. That is a weaker close, and it is written down as one — send the
> recording and the `gamepad:` line whenever convenient and this banner gets the same footing as the
> others.
>
> The procedure stays, because gate 8 is the one gate of that spec no runner can close, and it says
> so in its own text: *"subjective check that the control feels responsive"*. Re-run it when the
> module underneath moves — this round alone added the ladder mode and moved `MoveState`. Everything
> mechanical about this sample is already pinned elsewhere and does not need your machine — the route hash `0xfead7a87477a9258` on three OSes
> (`game_platformer_sim_test`), the camera and the drawn geometry (`game_platformer_view_test`), the
> layout-to-intent mapping (`game_platformer_input_test`). What is left is a hand on a key and an
> eye on a screen.

The live target is behind `IDE_POC`, so CI never builds it — configure with the full option set:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target game_platformer
./build/game_platformer
```

Windows: `scripts\win-dev.bat` sets up vcvars, and the binary lands next to the copied WebGPU DLL,
so run it as `build\example_ugly_game\game_platformer.exe` from the same prompt.

The window is 960×720 and the view inside it is 320×240 at ×3 — the level is 640×240, that is to
say **wider than the view on purpose**: a camera that never has to scroll is a camera whose clamp is
untestable. Everything is flat tinted quads, no art: grey-blue is solid, green is a slope, amber is
a one-way platform, cyan is the moving platform, red is the hero.

Startup prints exactly one line, and the pad half of it is the fact worth reading:

```
[platformer] WASD/arrows = move | space/up = jump | down+jump = drop | gamepad: GameController.framework (macOS) | Esc = quit
```

`gamepad:` says `evdev (Linux)`, `XInput (Windows)` or `none` on the other machines. Esc — or the
window button — ends the run with:

```
[platformer] window clean exit
```

Anything else on stderr is a finding, not noise: `level unreadable` means the bundle next to the
binary is stale, `controls unavailable` means the `input` section lost the `jump` action, and
`surface texture status <n> - frame skipped` repeating every frame means the surface never
recovered from a resize or a display change.

**Walk the level left to right and answer six questions.** They are the same six moves the scripted
run makes, which is the point: the hash says the moves came out identical on three machines, and it
says nothing at all about whether any of them is pleasant.

1. **Running and stopping.** Does the hero start and stop when you ask, or does he skate past the
   spot you released at?
2. **Jumping.** Hold the button and he goes higher than a tap — is the difference usable, and does
   the jump come out when you press it a hair before landing (the buffer window) or a hair after
   running off an edge (coyote)? Both are measured in ticks and pinned by tests; what is not pinned
   is whether the numbers feel right.
3. **The slope.** The green 45° hill: walk up it, walk down it, stop on it. Going down, does he stay
   glued to the surface, or does he leave the ground every few tiles and stutter?
4. **The one-way platforms.** The two amber slabs overlap in X on purpose. Jump *through* the lower
   one from below and land on top; then press down + jump to drop back through. Does the drop happen
   on the first press, and does he ever catch the platform he was told to leave?
5. **The moving platform.** The cyan slab shuttles left and right. Step onto it and stand still: it
   should carry you with no input of your own, and stepping off should hand you back your own
   momentum rather than fling you.
6. **Walls and ceilings.** Push into the right wall and into the overhang above the pocket. Does he
   stop cleanly, or does he shudder, stick, or slide up the face?

If a pad is connected, answer 1–3 again on the stick: the dead zone is circular (0.18) and shared
between the two axes, so a diagonal push is where a wrong shape shows up first — a diagonal that
reads as "drop through" is a bug in the sign of `move_y`, not a matter of taste.

**Record the screen** (spec #16 asks for it by name — macOS ⌘⇧5, GNOME Ctrl+⌥+⇧+R, Windows Win+G),
one pass of the level, thirty seconds is enough. Send the recording, the startup line with the
`gamepad:` field as it printed on that machine, and a yes/no per question above with a sentence
wherever the answer is no. A "no" here is not a failure of the gate — it is the number in
`default_profile()` that the gate exists to find.

## Beyond the gates

The six gates above are what the ADRs wait on. A machine with a screen, speakers and a pad can
also exercise things no gate covers — playing the sample game long enough to hear the audio, the
achievement toast surviving a restart, the offscreen `--demo` render path, an output device yanked
mid-frame, and `assetc` reproducing `bundle_hash = 0x8d38adffbcfd4a61` byte for byte on another OS.
Those scenarios, with the exact commands per platform, are sections A–F of
[`owner-setup.txt`](owner-setup.txt).

## What to send back

- `build/owner-report-<os>.txt` from each machine.
- `gate6-x11.png` and `gate6-wayland.png`.
- The probe's console output (the CONNECTED line, the conflict, the overlay-loaded line, the
  DISCONNECTED line).
- The loop timings per OS.
- The full `framework_physics_perf_test` output per OS — both the counter lines (they must match
  across all three) and the `worst=` / `mean=` numbers (they must not, and the spread is the point).
- The full `framework_character_perf_test` output per OS — the `small:`/`target:` counter lines must
  be identical to each other and to the other machines; the `worst=` / `mean=` numbers must not.
- The `perf_sweep.sh` table from the slowest machine you have — it, not the M3 Pro, decides how many
  iterations and how many bodies this engine claims.
- The platformer screen recording per OS, its startup line, and the six answers from section 6.

That closes gate 6 and 8 of spec #13, gate 8 of spec #14, gate 8 of spec #15 and gates 7 and 8 of spec
#16, which is what ADR
[0013](../.context/decisions/2026-07-27-desktop-dev-parity.md),
[0014](../.context/decisions/2026-07-28-framework-input.md) and
[0015](../.context/decisions/2026-08-08-physics-core.md) are waiting on to move from *Proposed*
to *Accepted*. Four are sent; the platformer is the outstanding half.
