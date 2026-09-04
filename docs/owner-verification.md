# Owner verification: the gates a runner cannot close

Seven of the twelve gates below are **closed** — each carries the run that closed it, with the
evidence. Three of the five open ones (§9, the effect library; §11, hot-reload in front of a person;
§12, the lit frame) opened with round #18 and wait for a machine with a screen; the other two (§13,
the network frame cost; §14, a live session) opened with round #22 and wait for two machines on one
network — §14 became runnable on 2026-09-04, when the peers learned to name each other, and it is
runnable in its convergence half only: the peer still has no window. They stay here as the procedure, because each one needs a
machine a CI runner is not: a real desktop session, a real GPU driver, a real gamepad. A gate is
re-run when a commit touches what it covers; the right-hand column names that surface.

| Gate | Spec | Where | Closed | Re-run when a commit touches |
|---|---|---|---|---|
| Linux X11 **and** Wayland — editor renders, gizmo moves an object | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 6 | Linux | 2026-08-05 | window backend, surface glue, `LINUX_WAYLAND` |
| End-to-end: clone → build → editor → edit game code → change visible | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 8 | Linux **and** Windows | 2026-08-05/06 | build loop, watcher, hot-reload, `win-dev.bat` |
| Live input: pad passport, profile, runtime rebind, unplug mid-session | [#14](../.context/specs/2026-07-26-framework-input.md) 8 | Linux **and** Windows | 2026-08-07 | `engine/input` backends, presets, profiles |
| Physics frame cost against a real frame budget | [#15](../.context/specs/2026-07-26-physics-core.md) 8 | Linux **and** Windows | 2026-08-22 | `engine/framework/physics`, load scenes, solver iterations |
| A target-size level costs a small one's tick, and that tick fits a frame | [#16](../.context/specs/2026-07-26-character-tilemap.md) 7 | Linux **and** Windows | 2026-09-01 | `engine/framework/character`, `engine/framework/tilemap`, query window |
| The platformer sample plays: slope, one-way, moving platform, and it feels responsive | [#16](../.context/specs/2026-07-26-character-tilemap.md) 8 | **all three** | 2026-08-30, re-closed with artefacts 2026-09-01 | `engine/framework/character`, `engine/framework/tilemap`, `example_ugly_game/platformer_*` |
| The samples look the same after being moved onto the graphics framework | [#17](../.context/specs/2026-07-26-graphics-framework.md) 9 | **any one** | 2026-09-02 | `example_ugly_game/platformer_view.*`, `example_ugly_game/fx*`, `example_ugly_game/sprite_out.*`, `engine/framework/graphics` |
| The effect library draws all three effects, and they are what the material says | [#18](../.context/specs/2026-07-26-materials-shaders.md) 1 | **macOS** (the reference is pinned on Metal) | — | `engine/material/library/*`, `engine/material/cache.cpp`, `engine/render/material_*` |
| A shader edit lands without a restart, and a broken one leaves the picture alone | [#18](../.context/specs/2026-07-26-materials-shaders.md) 3 | **any one** with a screen | — | `engine/material/hot_reload.cpp`, `engine/material/reload.cpp`, `tools/ide/editor/material_panel*`, `example_ugly_game/material_fx.*` |
| Five lights out of a table light the scene, and the light is where the data says | [#18](../.context/specs/2026-07-26-materials-shaders.md) 7 | **macOS** (the reference is pinned on Metal) | — | `engine/light/*`, `engine/render/light_*`, `engine/render/shaders_light.cpp` |
| The network frame — rollback, recording and socket — fits a real frame budget | [#22](../.context/specs/2026-09-02-deterministic-net.md) 8 | **the slowest machine you own** | — | `engine/net/*`, `engine/framework/rollback/*`, `example_ugly_game/platformer_peer*` |
| A live session between two machines: one input, one state over a real wire (windows still missing) | [#22](../.context/specs/2026-09-02-deterministic-net.md) 9 | **two machines on one network** | — | `engine/net/*`, `engine/framework/rollback/*`, `example_ugly_game/platformer_peer*` |

The last to close was the look of the sample after the framework move, and it is the kind a runner
cannot even *print* — it is recordings held side by side, one pair per sample. The character tick
cost stood beside it until 2026-09-01 and was the other kind: an answer a runner could print but not
*judge*, because it asks, as the physics gate does, whether a number fits a real frame budget on the
slowest machine you own.

**Both sessions ran on 2026-09-01 — Linux first, Windows after — and between them they left one
gate.** On the Intel UHD 620 box under Nobara: the tick cost measured (§5, `worst` 0.2600 ms —
1.56% of a frame), the platformer played and its three findings fixed (§6, closed with the
recording), the reference frame compared against a real Intel Vulkan driver (§8, `blob 1` of an
allowed 12). On the same box under Windows 11 and MSVC: the tick cost measured again and §5 closed
on it (`worst` 0.2338–0.6174 ms, the counters identical to the Linux run), and §8 answered twice
more — once on the discrete NVIDIA MX150 and once, through `LIKENES_GPU_POWER=low`, on the Intel
driver, a different stack over the same silicon. Gate 9 (§7) was what was left, and it is the one that
could not be finished from either OS alone in a single pass — it needs your eyes on two pairs of
recordings. **It was answered on 2026-09-02** from the Windows box: four binaries against the two
"before" worktrees prepared there, both halves in one evening. Its Linux pass would now be a re-run,
not a first answer, and both worktrees stand ready on that box too.

The right-hand column is the whole point of keeping the procedure: a closed gate protects nothing if
the code under it moves and nobody re-runs it — and the platformer gate, closed 2026-08-30, sits
directly under the module this round keeps changing.

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

**The exclusion is not always the answer, and 2026-09-01 found the second mechanism.** On that run
the tree was already excluded — `Get-MpPreference` listed it — and a subset of freshly built
binaries still came back 126. Run one through `cmd` instead of the shell and Windows names the real
blocker: *"blocked by the Device Guard policy of your organisation"*. That is **Smart App Control**,
user-mode code integrity judged against a reputation service, and it has nothing to do with the
antivirus exclusion list. It blocks *some* unsigned binaries and not others — on that machine
`determinism_test.exe` ran and `audio_golden.exe` did not, same build, identical permissions — so
the symptom reads as a flaky gate rather than as a policy, which is why it is written down here.

```powershell
Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' |
  Select-Object VerifiedAndReputablePolicyState   # 0 off, 1 enforcing, 2 evaluation
```

Turning it off is Settings → Privacy & security → Windows Security → App & browser control → Smart
App Control. **That switch is one-way** — Windows cannot re-enable it without reinstalling the OS —
so it is the owner's call, and nothing in this repository touches it. Left on, the targets it
refuses stay `BLOCKED`, which is the honest verdict: the machine has said nothing about them.

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

> **Closed 2026-09-01** by the Windows run at the foot of this banner. The counter half of this gate
> is closed by CI and needs no machine of yours: the same
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
>
> **The slowest machine answered on 2026-09-01, and it answered Linux.** Nobara (kernel 7.2.0,
> clang, Release) on the Intel UHD 620 box — the same box that decided the body count in the
> physics gate one section up — put the target-size level at `worst` **0.2600 ms** and `mean`
> **0.0242 ms**: **1.56%** and **0.15%** of a 16.67 ms frame. That worst tick is one draw from a
> spread: five idle runs of the same binary landed between 0.1910 and 0.2785 ms, and the `mean`
> barely moved at all (0.0226–0.0290). Three times the M3 Pro's worst tick
> and still sixty-four frames' worth of headroom, so the number that would have changed the spec
> does not exist here — unlike #15, where the owner's run cut 500 bodies to 350. The counter half agreed
> with itself across the thirty-two-fold area on this machine too: `queries=5345 scanned=38794`
> and `hash=5b3bcf0fc03ada62` on both maps, `allocs=0`.
>
> **Windows on that same box answered 2026-09-01, and it is what closes the gate.** Windows 11
> (build 26200), MSVC 14.44.35207, Release, the same i7-8550U: five idle runs put the target-size
> level at `worst` **0.2338–0.6174 ms** and `mean` **0.0304–0.0310 ms** — 0.18% of a 16.67 ms frame
> by the mean, 1.7% by a typical worst tick, 3.7% by the longest of the five. The counters came out
> identical to the Linux run on this silicon down to the digit: `queries=5345 scanned=38794`,
> `hash=5b3bcf0fc03ada62`, worst tick `20 queries / 156 tiles`, `allocs=0`, `cols=[106, 193]`,
> `ground=712 air=1088 ceiling=14 slope=123`. Two toolchains agreeing about the arithmetic to the
> unit and differing only in the clock is the invariant answering on a third OS, not a coincidence.
> MSVC is about a third slower than clang on this box by `mean` (0.0305 against ~0.0227) and carries
> a longer tail: the 0.6174 ms draw is one sample of five whose `mean` did not move (0.0310 against
> 0.0305 beside it), which is the scheduler, not the tick. Three OSes measured, the slowest of them
> leaving twenty-six frames of headroom — no number here would have changed the spec, unlike #15,
> where the owner's run cut 500 bodies to 350.
>
> The box was **idle** for those five runs, and that is why they are quoted: `LoadPercentage` was
> held under 12% for five consecutive samples before the first one started. Straight after the build
> it read 60–63% while Defender finished reading the fresh binaries, and the section below explains
> what measuring through that would have printed.

This gate is the character half of the frame-cost stage of `owner_check.sh`, so a full report
already carries it. Alone:

```sh
cmake --build build --target framework_character_perf_test
./build/framework_character_perf_test
```

(Windows: `scripts\win-dev.bat check` builds the tree, then
`build\framework_character_perf_test.exe`. Both frame-cost measurements at once, physics and
character, are `bash scripts/owner_perf.sh`.)

**Measure on an idle machine — that is part of the procedure, not a nicety.** On 2026-09-01 this
same binary on this same box printed `worst=3.7349 ms mean=0.1579 ms` while three builds were
running beside it, and `worst=0.1910…0.2785 ms mean≈0.0227 ms` with the box quiet: **fourteen times
apart**, 22% of a frame against 1.5%. Nothing in the output separates the two runs — the counters,
the worst tick and the hash are pinned in the binary and come out identical either way, so the
loaded run prints `PASS` and hands you a timing that reads as a finding about the tick. The step
above this one in `owner_check.sh` builds nothing, but a build you started yourself in another
window counts. Close them first, then measure.

Expected output. The counters are **not repeated here**, for the reason given one section up: they
are pinned as constants inside the binary and a copy in a runbook is a copy nothing checks. A
differing counter prints its own `FAIL: <field> = N, reference says M` line and the run exits
non-zero.

```
framework character perf gate (1024 x 256 tiles)
  small: map=256x32 (8192 tiles) queries=<n> scanned=<n>
  small: worst tick = <n> queries / <n> tiles, hash=<hash>
  small: worst=<time> ms mean=<time> ms allocs=0 cols=[<lo>, <hi>]
  small: ground=<n> air=<n> ceiling=<n> slope=<n>
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
> **Re-closed 2026-09-01 with artefacts,** on Nobara (X11), and this banner now stands on the same
> footing as the four gates above. The recording is
> `~/wiki/_meta/attachments/2026-09-01-like-nes-gate8-platformer-linux.mp4`, one pass of the level;
> the startup line read `[gpu] Intel(R) UHD Graphics 620 (KBL GT2) | Vulkan | BC: yes` and
> `gamepad: evdev (Linux)`, and the run ended on `[platformer] window clean exit` with nothing else
> on stderr. Questions **1–6 came back clean** and question **7 clean on both halves** — but only
> after three findings the two runs produced, each written up below with its numbers and its gate.
> The last of them was re-checked in a live window on the fix, and the owner's verdict is the
> boundary of the behaviour, not a shrug: *"the platform no longer throws you out by accident, only
> when it squeezes you against a wall."* Being squeezed against a wall is the lethal case he chose
> himself when he picked "shove him along in front of it" over "let him pass through" — so a crush
> there is the sample obeying its own design, and a crush anywhere else is a finding.
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
so run it as `build\game_platformer.exe` from the same prompt.

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

**Walk the level left to right and answer seven questions.** The first six are the moves the
scripted run makes, which is the point: the hash says they came out identical on three machines, and
it says nothing at all about whether any of them is pleasant. The seventh is here because the
scripted run did *not* make it — it was added on 2026-09-01 after the live run found three defects
the hash had been passing over for a month.

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

7. **The edges of the level and the platform's flank.** Walk into the left edge of the map and jump
   at it — the hero must stay on the level. Then stand in the cyan slab's path, in mid-air, and let
   it arrive: it should shove you along in front of it, not swallow you.

If a pad is connected, answer 1–3 again on the stick: the dead zone is circular (0.18) and shared
between the two axes, so a diagonal push is where a wrong shape shows up first — a diagonal that
reads as "drop through" is a bug in the sign of `move_y`, not a matter of taste.

### Run 2026-09-01 (Linux, Nobara): two findings, both fixed and pinned

The owner ran the sample and answered 1–6; questions 1–5 came back clean and question 6 clean for
walls and the overhang. Two things came back as findings, and question 7 above exists because of
them — both were reachable by ordinary play and neither was visible from any gate that existed:

* **The level had no left edge.** *"I walked left off the screen and the hero vanished, but the
  controls stayed — I could still move the stage."* The left wall of the map is three tiles tall
  and the jump takes four, so hero goes over it, and past column zero the grid holds no tiles at
  all: he kept walking, kept falling and kept being controlled, outside the level. Fixed in
  `platformer_scene.cpp` — the sample clamps the hero to the same rectangle the camera is already
  clamped to (X only; the floor spans the whole width, and catching a fall through it would hide a
  controller defect instead of closing a hole in the level). The clamp puts him **on** the boundary,
  which is the edge of the map and not a promise of empty space, so it is followed by a nudge one
  tile at a time back inside while the hull still stands in solid tiles — on this map the edge
  column is exactly that, and without the nudge the clamp parked him inside the wall. Pinned by the
  edge run in `game_platformer_sim_test`, which walks left **with the jump held** — plain walking
  stops at the wall on 24.125 and would have passed the assertion without ever reaching the edge —
  and which asserts on the run's **leftmost** position, not its last: the nudge returns him inside,
  so by the final number "flew over the wall and was returned" and "never left the wall" look the
  same.
* **The moving platform went through a character it was not carrying.** *"If you jump neatly just
  before the blue platform arrives, it keeps moving and squeezes you upwards like paste out of a
  tube."* The character tick looked at moving bodies only through the support it was standing on, so
  a platform arriving from the side kept driving into a mid-air character — fifty units deep over
  half a second — and the only thing pushing back was `move_and_slide` stepping off the contact by
  `SKIN` along whichever axis he had sunk into **less**, an eighth of a unit per tick, across the
  platform's travel. Fixed by `character/push.hpp`: a body driving into a character shoves him along
  its own direction, atomically, and reports `crushed` when the destination is blocked by something
  other than the pusher. The ladder tick gets the same step 0, because a platform arriving at a
  character hanging on a ladder is the same event. Pinned by `framework_character_push_test` (six
  cases; on the pre-fix code the first of them measures a 53.985-unit penetration, and the
  bystander case — a still body the character merely touches, with a **lower key** than the pusher —
  is what defeats answering the question with a single nearest-hit sweep). Being crushed stays a
  statement, not a decision: the engine sets the flag, and the sample answers it by returning the
  hero to the spawn point, because left where he stands he would be crushed again next frame.

Neither fix moves the route hash `0xfead7a87477a9258`: the scripted route touches neither the map's
edge nor the platform's flank, which is exactly why it said nothing about either.

### Re-run 2026-09-01 (Linux, Nobara): the platform's flank, third finding

The owner replayed the level on the two fixes above and recorded it. Questions **1–6 came back
clean, all six**, and question 7 split: *"the edges are clean, the platform is not"*. What he saw
was not a flag but a teleport — *"it throws me back to the spawn point"* — best visible at **11–13 s**
of the recording, and the sample only ever teleports for one reason: `crushed` is set and
`platformer_scene.cpp` answers it with `place_at_spawn`.

Nothing was crushing him. **The carry was atomic across both axes at once.** A rider standing on the
slab's roof is pushed right along with it; the overhang's left face is at x 592 and the slab's roof
ends at 592 as well, so the rider ends up flush against that face at x 583.875 with a hull half-width
of 8 — and there he still has a one-unit carry owed to him every tick. `carry_by_support` asked
"does the whole displacement fit" in one question, got `false` because the face is in the way, and
reported failure, which the tick reports as `crushed`. Repro before the fix, one tick:
`t 1 CRUSHED hero=(40.000, 207.875)` — the coordinates are already the spawn point.

The two axes are not the same event, and folding them into one question is what made the sample
lethal. Upward, a platform **presses** the rider into the ceiling: nowhere to go is genuinely a
crush. Sideways, it merely **drives along** under his soles: nowhere to go is a slip, the ordinary
thing that happens whenever a rider meets a wall. Fixed in `carry_by_support` — the carry stays
atomic, but **per axis**: a blocked horizontal drops to a slip and `true`, and only a blocked
vertical is still a refusal. The ceiling case (`test_the_crush_is_a_fact`) is untouched, which is
the point of the pair.

Pinned twice, both red on the pre-fix code and green after it:

* `framework_character_platform_test` — `test_a_blocked_ride_is_a_slip`, three assertions on a
  fixture with a pillar planted in the platform's path: the blocked rider is not crushed, keeps his
  ground, and stops at the last whole carry step before contact (14, the step before 16); the pair
  without the pillar rides on; and a diagonal carry proves the vertical component is still delivered
  while the horizontal one is refused. Pre-fix: three FAILs.
* `game_platformer_sim_test` — `test_the_rider_is_not_crushed_at_the_overhang`, on the shipped
  bundle, the sample's own numbers: `rider: hero=583.542 plate=555.999 slipped=1 crushed=0`. The
  `slipped` field is the precondition, not decoration — without it the assertion would also pass for
  a hero the platform never reached. Pre-fix the same line reads `hero=148.967 slipped=0 crushed=1`,
  and 148.967 is the fall away from the spawn point the owner watched.

The route hash `0xfead7a87477a9258` is unmoved again, for the same reason as before: the scripted
route never rides the slab into the overhang.

One thing the run **did not** find, recorded because it was chased and ruled out: the pocket the slab
crosses is not too low for a standing hero. His crown sits at 191.875 (floor top 224, half-height 16,
minus the `SKIN` the controller keeps under his soles) and the slab's underside at 192 — a gap of
1/8, wider than `CONTACT_SLOP`, so nothing touches. A no-jump sweep of every start column from 20 to
630, both travel directions, reports `crushes=0` after the fix. The crushes that remain all require
jumping into the slab's band next to the step, which is the lethal case the owner asked for by name
when he chose "shove him along in front of it" over "let him pass through".

The owner re-ran the level in a live window on the fix and closed question 7: *"the platform no
longer throws you out by accident, only when it squeezes you against a wall."* That second half is
not a leftover defect — it is the crush he asked for by name in the finding above, arriving where
it was designed to arrive.

**Record the screen** (spec #16 asks for it by name — macOS ⌘⇧5, GNOME Ctrl+⌥+⇧+R, Windows Win+G),
one pass of the level, thirty seconds is enough. Send the recording, the startup line with the
`gamepad:` field as it printed on that machine, and a yes/no per question above with a sentence
wherever the answer is no. A "no" here is not a failure of the gate — it is the number in
`default_profile()` that the gate exists to find.

## 7. Gate 9 of #17 — the sample looks the same after the framework move

> **Closed 2026-09-02** on the Windows box — both halves in one pass, the records at the foot of
> each half below. Vertical 3 moved both samples onto `engine/framework/graphics`: step A took the
> platformer's camera, view window, tile drawing and draw order, step B3 took the shooter's
> particles and the instance buffer under them. Everything mechanical about either move is pinned
> headless — `game_platformer_view_test` and the untouched route hash `0xfead7a87477a9258` (gate 10)
> for the first, `game_fx_test` and `game_sprite_out_test` for the second — and what no runner can
> say is whether the picture on the screen is the same picture. That is two recordings and your
> eyes, **twice**: the two halves below are separate runs against separate "before" commits, and
> either can be answered without the other.

### The platformer, after step A

Two builds of the same level: the commit before the move, and the current one. The old one lives in
a worktree so your checkout stays where it is.

**The "before" build plays worse, and that is not what this half is asking about** — the shooter
half below names its deliberate differences up front, and this one has three of its own. `2bdfcb7`
predates the fixes of 2026-09-01, and the owner's 2026-09-02 pass hit all three: walking left off
the map leaves the hero outside the world, alive and controllable (`e01908d`); a platform arriving
from the side drives into him, *"squeezes you upwards like paste out of a tube"* (`d0d4a14`); and
riding the plate into the overhang sets `crushed`, which the sample answers by teleporting him to
the spawn point (`43b0ed6`). Not one of the three is a picture — they are the subject of gate 8 of
#16 in section 6 above, and they are here only because this half's "before" commit sits in front of
them. Two consequences for the run itself: the ride in question 4 is cut short at the overhang in
the "before" recording, and the left leg of question 1 is walked out of the map instead of stopping
at the edge. Compare what is drawn, not how far he gets.

```sh
git worktree add ../like-nes-before-platformer 2bdfcb7
cmake -S ../like-nes-before-platformer -B ../like-nes-before-platformer/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ../like-nes-before-platformer/build --target game_platformer
cd ../like-nes-before-platformer && ./build/game_platformer          # "before"
```

Run it **from the worktree root**, not from `build/`: the bundle is found relative to the working
directory. Then the same three commands in your own checkout for the "after" run, and
`git worktree remove ../like-nes-before-platformer` when both recordings are in hand.
**On the Linux box the worktree and its build already exist** (prepared 2026-09-01), so the Linux
pass is `cd ../like-nes-before-platformer && ./build/game_platformer` and nothing else. **On the
Windows box they exist too** (prepared 2026-09-02, MSVC 14.44, Release): `cd ..\like-nes-before-platformer`
and `build\game_platformer.exe`. Targets land flat in `build\`, next to the copied `wgpu_native.dll` —
there is no `build\example_ugly_game\` binary, that directory holds CMake bookkeeping only. Building
a worktree yourself goes through **that worktree's own** `scripts\win-dev.bat` (it works from its own
root, so the checkout's copy would rebuild the checkout), and it builds every target, not one.

The worktree is named after its half on purpose: the shooter half below pins a **different** "before"
commit, and one shared `../like-nes-before` would have forced the two halves into sequence — build,
watch, tear down, build again — for no reason other than the name. They are independent runs and the
text above already says so.

Both runs print the same startup line — the one from section 6, `gamepad:` and all — and end with
`[platformer] window clean exit`. A difference in either line is a finding before you look at a
single frame.

**Record one pass of the level in each build** (macOS ⌘⇧5, GNOME Ctrl+⌥+⇧+R, Windows Win+G), walking
left to right: spawn, the green hill, both amber slabs, the cyan platform, the right wall. Then hold
the two recordings side by side and answer:

1. **The camera.** At the far left and far right the view stops dead at the level edge. Does it stop
   at the same place in both, and does it start scrolling at the same point on the way there?
2. **The hill.** The green slope is a staircase of 16 sub-quads, each one pixel wide, rising left to
   right and standing on the tile floor. Same staircase, same direction, no gap under it?
3. **The slabs.** Amber, and *only* the one-way platforms are amber — a slab drawn grey-blue like a
   wall is the exact bug the per-kind tint exists to prevent.
4. **The platform and the hero.** The cyan plate shuttles; standing on it, is the hero drawn **over**
   the plate, with his feet visible, in both runs?
5. **The edges of the screen.** Nothing half-drawn or popping at the left and right borders as the
   camera moves: a tile column either fully enters the view or is not drawn at all.

The tints themselves moved from four floats to packed RGBA8, so each channel is now the nearest
8-bit value to what it was — at most one step of 1/255, which is below what the framebuffer could
show either way. A colour difference you can actually see is therefore a finding, not the rounding.

Send both recordings, or one recording plus a "same as before" per question. A "no" on question 1 or
5 points at the view window (`platformer_view.cpp`), a "no" on 3 at the tint table
(`TileSet::tint`), a "no" on 4 at the layer constants — each of those has a headless gate that
should have caught it, so the answer also names a hole in `game_platformer_view_test`.

**Answered 2026-09-02** on the Windows box (MSVC 14.44, Release, Intel UHD 620), both builds run
from their own roots. Questions **2, 3, 4 and 5 came back clean point by point**: the green
staircase rises left to right with no gap under it, amber sits on the one-way slabs and nowhere
else, the hero is drawn over the cyan plate with his feet visible, and no half-drawn tile column
appears at either screen edge. Question 1 rides on the run's blanket verdict rather than a
side-by-side answer — nothing was reported against the camera, and the "before" run's left leg is
not comparable anyway for the reason given at the top of this half. The "before" run reproduced the
three gameplay defects listed there and nothing besides, which is the **first sighting by eye** of
what `d0d4a14`, `e01908d` and `43b0ed6` fixed: until this run all three were pinned headless only.
The shooter half below was answered the same evening.

### The shooter, after step B3

Step B3 moved the shooter's particles onto the framework emitter and its instances through a new
adapter. Unlike the platformer half above, **this one is not expected to be frame-identical, and
saying so up front is the point** — three differences are deliberate, and mistaking one of them for
a regression costs an evening:

- **Trajectories differ.** The old system drew its random numbers from a float LCG; the framework
  draws from a `fix32` one, and it damps velocity before the move rather than after. Individual
  sparks therefore fly along different paths. What must NOT differ is the character: same colours,
  same rough size, same reach, same time to fade.
- **The ship's exhaust no longer fans with its offset.** The old trail correlated a particle's
  vertical speed with where in the 10-pixel band it was born. It is now a band plus a narrow cone,
  which looks the same in motion but is not the same arithmetic.
- **Particles no longer rotate individually.** They are drawn with the star, which is a radial
  gradient — `game_fx_test` asserts it is symmetric under both mirrors and under transposition, so
  a rotated star is the same star. If you can see a difference here, that assertion is wrong and the
  gate has a hole.

**All three sit below the threshold of the eye, and the expected answer to this half is therefore
"the two are visually the same game".** That sentence was missing until 2026-09-02, and its absence
was a defect of this runbook, not of the reader: "mistaking one of them for a regression costs an
evening" reads as a promise that they are *visible*, and sends you hunting for something the port
was built not to produce. Every number that governs what the eye sees was carried across unchanged —
the burst counts (4 / 16 / 5 / 46 + 14 / 12) and speeds (120 / 250 / 170 / 340 / 140 / 220 px/s) now
in `fx_table.hpp` are the literals that stood in `fx.cpp` at `ddd0efa`, and so are the colours, the
sizes, the lifetimes, the ±5 px trail band and the 0.9 damping. What actually changed is the random
stream, and one correlation traded for a ±4.8° cone of the same vertical reach. So the six questions
below are not "spot the difference": they are six named things that had to **survive**, and a
blanket "same game" answers each of them yes.

```sh
git worktree add ../like-nes-before-shooter ddd0efa
cmake -S ../like-nes-before-shooter -B ../like-nes-before-shooter/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ../like-nes-before-shooter/build --target game_sidescroller
cd ../like-nes-before-shooter && ./build/game_sidescroller          # "before"
```

**Prove first that the two binaries are not the same one.** Neither build prints anything that
names itself — no version line, no banner — so "I see no difference" is by itself indistinguishable
from having run one binary twice, which is the vacuous green this repo hunts everywhere else. The
positive control needs no code and no eye:

```sh
./build/game_sidescroller --golden-selftest
```

`--golden-selftest` was added after `ddd0efa`, and the old build's argument loop ignores what it
does not know. So the **new** build answers in the console and exits without opening a window —
`golden control: PASS (blot, scatter and one-pixel shift refused)`, then `golden repeat: PASS` — and
the **old** build starts the game as if the flag were not there. A window means you are standing in
the worktree; two PASS lines and a prompt back mean you are standing in your checkout. There is no
third outcome, and getting the same result from both paths means one of them is not the tree you
think it is.

The weaker check, if you want it anyway: the two files differ in size — 2 723 328 and 2 817 536
bytes on the Windows box of 2026-09-02 — and `md5sum` settles it if they ever match.

Same shape as above: run from the worktree root so the bundle resolves, then the same three commands
in your own checkout, then `git worktree remove ../like-nes-before-shooter`. This worktree and its
build are prepared on the Linux box too, from the same 2026-09-01 pass, and on the Windows box from
the 2026-09-02 one: `cd ..\like-nes-before-shooter` and `build\game_sidescroller.exe`, same flat
layout as the half above.

**Record one run of each** up to and including the boss fight, then answer:

1. **The ship's exhaust.** A continuous band of blue-white sparks trailing off the ship's left edge,
   about ten pixels tall — not a single line, not a cone starting from a point.
2. **Firing.** Every shot puffs four pale-cyan sparks at the muzzle, and the bullet drags a short
   trail behind it. Same brightness in both runs?
3. **An enemy dying.** Sixteen orange sparks, spreading in every direction, the shortest-lived gone
   in a third of a second and the longest in eight tenths. Same colour, same reach, same spread of
   fade times — the cloud must not go out all at once?
4. **The boss dying.** The big one: sixty sparks in two waves — 46 orange ones thrown fast and far,
   and 14 bigger, paler, near-white ones that stay closer in. Both waves present, and the pale one
   *not* outrunning the orange one? (It is slower on purpose: 140 against 340 px/s.)
5. **Glow.** The sparks are visibly BRIGHTER than the sprites around them — that is the `MAT_Glow`
   exposure of 1.9 that used to be a literal in `fx.cpp`. Flat, unglowing sparks mean the material
   exposure is not reaching the instance.
6. **Nothing disappears under load.** This one your eyes answer badly by construction — it asks for
   the ABSENCE of a moment, and not seeing one is not the same as there not being one. So since
   2026-09-02 the sample answers it itself: on exit it prints

   ```
   [game] fx: peak 136 of 512, dropped 0
   ```

   — the owner's run of 2026-09-02, and the answer to this question. `dropped` **is** the question:
   anything but `0` means a burst was truncated because the pool was full, which is exactly the
   "sparks stopped coming" this asks about. `peak` is the headroom that zero sits in. Three measured
   numbers for scale — 240 frames of idle flying peak at 32, the scripted fight of `game_fx_test` at
   113, and a played fight through the boss at 136, which is 27% of the pool. Play a dense fight,
   quit with Esc, read the line. A counter stuck at zero would be the worst outcome here, so both halves of it are pinned:
   `game_fx_test` asserts `fx.peak()` equals the high-water mark the test measures for itself, and
   `framework_graphics_particle_refusal_test` asserts `dropped()` counts an overflow one particle at
   a time.

A "no" on 1 points at `spawn_half` in the ship's trail description, on 3 or 4 at the burst counts in
`fx.cpp`, on 5 at `material_exposure` in `sprite_out.cpp`, and on 6 at `FX_CAP` — which the
line on exit now reads out, so that question is no longer judged by eye.

**Answered 2026-09-02** on the Windows box (MSVC 14.44, Release, Intel UHD 620), one run of each
build up to and through the boss fight: *"no difference at all, two visually identical games"* —
which is questions 1 through 5 each answered yes, the exhaust band, the muzzle puffs with the
bullet's trail, the sixteen orange sparks, both waves of the boss's sixty, and the glow, all as
described. Question 6 rides on that verdict instead of answering it: it asks for the **absence** of
a moment, and the owner reported no anomaly rather than reporting that he watched for one — the pool
holding under a live boss fight was unrefuted, not tested. So the exit line was added the same day
and the fight replayed: **`[game] fx: peak 136 of 512, dropped 0`**, which answers question 6 by
measurement. What that number settles is this run and the size of its margin — 136 against a
scripted 113, so a played fight is denser than the script by a fifth and still leaves nearly four
fifths of the pool unused. What it does not settle is every possible fight; the difference from
before is that any future one now reports itself instead of being watched for. Provenance of the pair rests on the owner's own
navigation into the worktree: the `--golden-selftest` control above was written **after** this run,
in answer to it, so this pass did not use it. What corroborates the pass instead is the platformer
half, run the same evening through the same mechanism, which returned two **different** results — a
mechanism handing back one binary twice could not have done that. Every run from here uses the
control.

With both halves answered, gate 9 is closed.

## 8. Gate 2 of #17 — the reference frame on a real GPU

> **Machine-side, and green on three runners.** The gate itself is automated: the shooter renders
> frame 239 of the scripted run and compares it to `example_ugly_game/golden/scene_960x540.png`,
> pinned on Metal. What CI cannot say is whether the tolerance survives a *real* driver — the
> runners have lavapipe on Linux and DX12-WARP on Windows, both software rasterisers, and the
> macOS runner is the same Metal the reference was baked on. Your three machines are the only
> AMD/NVIDIA/Intel in this project.
>
> **First real driver answered 2026-09-01: Intel UHD 620 (KBL GT2), Vulkan, Nobara.** The
> tolerance survives it, and with room to spare — `golden frame: mean 0.00000 max 0.18039
> over-eps 0.0004% blob 1` against an allowance of `eps 0.020, frac 0.100%, blob 12`. One blob of
> twelve, four ten-thousandths of a percent of the pixels: the reference baked on Metal is a
> reference on an Intel Vulkan driver too, which is the thing no runner could say.
>
> The strict half came out **exact**: `golden repeat: mean 0.00000 max 0.00000 over-eps 0.0000%
> blob 0` — two runs on this adapter agree bit for bit, so nothing here is about the engine. The
> self-test refused blot, scatter and the one-pixel shift, so the comparison that passed is a
> comparison that can still fail.
>
> **NVIDIA and Windows answered 2026-09-01, on that same box.** Windows 11 (build 26200) takes the
> discrete adapter by default, so the first run was the one no machine in this project had made:
> `[gpu] NVIDIA GeForce MX150 | Vulkan | BC: yes`, `golden frame: mean 0.00000 max 0.03137 over-eps
> 0.0002% blob 1` — one blob of an allowed twelve, two ten-thousandths of a percent of the pixels.
> Forcing the integrated adapter with `LIKENES_GPU_POWER=low` answers the half the silicon alone
> could not: the *same* Intel UHD 620, under the Windows driver instead of Mesa, came out `max
> 0.00392 over-eps 0.0000% blob 0` — **not one pixel past eps**, against `max 0.18039 over-eps
> 0.0004% blob 1` from Mesa on that very card. Two drivers over one GPU differing by a factor of
> forty-six in peak error, both green, is the measurement that says the tolerance is sized for the
> stack and not for the silicon. `repeat` was exact on both adapters (`max 0.00000 blob 0`) and the
> self-test refused blot, scatter and the one-pixel shift on both, so neither number is about the
> engine.
>
> **AMD is still unanswered** — it is the one adapter vendor nothing in this project has run on.

```sh
./build/game_sidescroller --frames 240 \
  --golden example_ugly_game/golden/scene_960x540.png --golden-selftest
```

Expected, three lines in this order:

```
[game] golden control: PASS (blot, scatter and one-pixel shift refused)
[game] golden repeat: mean 0.00000 max 0.00000 over-eps 0.0000% blob 0 (eps 0.000, frac 0.000%, blob 0)
[game] golden repeat: PASS
[game] golden frame: mean <small> max <small> over-eps <small>% blob <n> (eps 0.020, frac 0.100%, blob 12)
[game] golden frame: PASS
```

`repeat` is the strict one and it has no tolerance at all: two runs on the same adapter must agree
exactly. If *it* fails, the finding is about this engine, not about your driver — send the numbers
before looking at anything else.

What `frame` judges is `blob` — the largest CONNECTED patch of differing pixels — and not the peak
error, because on lavapipe the peak is already 0.73: an edge pixel a rasteriser rounds to the other
side of a sprite outline differs by the CONTRAST of that outline, not by a small amount. A real
defect moves an area instead, so the patch is what separates the two. The cost of that is worth
knowing before you read a green line: a bright artifact SMALLER than 12 connected pixels is
something this gate cannot tell from rasteriser noise.

A red `frame` is not automatically a bug. Send the numbers line and the `golden_actual.png` the run
drops next to it: that file is the only evidence by which the tolerance either gets widened with a
measurement or stays and the renderer gets fixed. Do **not** re-bake with `--update-golden` — the
reference is pinned on Metal deliberately, and a re-bake on another GPU silently turns the gate into
a comparison of your machine with itself.

## 9. Gate 1 of #18 — the effect library on a real GPU

> **Open.** The numeric half of this seam is machine-side and green on all three runners: how many
> pipelines the warm-up builds, in how many draw calls the frame is assembled, and whether a frame
> after the warm-up compiles anything. What no runner can answer is the picture — whether flash,
> outline and dissolve look like what the material *says*, on a screen.

```sh
cmake --build build --target material_golden
./build/material_golden --golden engine/render/golden/materials_640x360.png
```

Expected, on the Metal machine the reference was baked on:

```
[material] no entry point `fs_nope` in the library: falling back
[gpu] <adapter> | Metal | BC: yes
  warm-up: 3 pipeline(s) for 7 material(s), 0 fallback(s)
  frame: 28 instance(s) in 3 draw call(s)
  painted: 30.4% of the frame
  golden engine/render/golden/materials_640x360.png: mean=0.00000 max=0.00000 frac=0.00000
material-gpu: PASS
```

The first line is not a failure: the run deliberately builds a material naming a shader entry that
does not exist, to prove a broken material still draws with the fallback instead of drawing nothing.
Its absence would be the finding.

`max=0.00000` means the reference file is bit for bit what your GPU just drew, so opening
`engine/render/golden/materials_640x360.png` *is* looking at your own frame. If the numbers came out
non-zero, write yours to a scratch path first — the harness bakes a reference it cannot read —
and compare the two by eye:

```sh
./build/material_golden --golden /tmp/mine.png
```

Do **not** point `--update` at the checked-in reference on another machine: it would replace the
pinned frame with yours and turn the gate into a comparison of your GPU with itself.

Then open the PNG and answer four questions — this is the half the numbers do not cover:

1. **Seven columns, four rows.** Left to right: `flash`, `flash_red`, `flash_gold`, `outline`,
   `outline_danger`, `dissolve`, `dissolve_ash`. Bottom row is the material's own value, each row
   up adds one step. Is every column there?
2. **Flash** — the tint grows upward, white / red / gold by column. The bottom sprite carries the
   material's own strength: untouched in `flash` and `flash_red` (0), already half-tinted in
   `flash_gold` (0.5). Do the colours match the names, and is the ramp the right way up? Strongest
   at the bottom means the parameter arrives inverted.
3. **Outline** — a ring OUTSIDE the silhouette, black in column 4 and red in column 5, starting at
   1 px in the bottom row (2 px for `outline_danger`, its own thickness) and growing a pixel per
   row. Does it ring the whole sprite, or only an arc? An arc means thickness is converted to UV
   through the wrong size — the bug this gate caught on 2026-09-02, when the shader offset by a
   screen-wide texel instead of the instance's own.
4. **Dissolve** — the bottom sprite is whole (threshold 0) and holes eat it upward, with a coloured
   rim on the edge: orange in `dissolve`, ash-grey and twice as wide in `dissolve_ash`. Is the rim
   there, and is it the colour the material names?

**On Linux and Windows run `--selftest`, not `--golden`.** The comparison this harness carries is
the render-vertical one (`eps 0.02`, `frac 1%`, peak cap 0.35), and it judges the PEAK error — which
§8 has already measured at 0.73 on a foreign rasteriser, on exactly the sprite edges this scene is
made of. A red `frame` there would be about the criterion, not about the renderer. The cluster-based
comparison that survives foreign drivers lives in the sample game (`example_ugly_game/frame_golden.*`)
and reaching it from the engine needs it moved first — a follow-up, not this round. What `--selftest`
does say on those machines is portable and worth having: the same counters, plus two runs on that
adapter agreeing bit for bit.

```sh
./build/material_golden --selftest
```

## 10. Gate 9 of #18 — the sample game plays with library materials

> **Open.** The library reaches the game through `library.bundle`, and everything a runner can say
> about that path it already says: the bundle matches its sources byte for byte on three OSes, and
> the run-splitting numbers are asserted headlessly. What no runner can answer is whether the
> effects land on the right objects at the right moment, in a game that is moving.

```sh
cmake --build build --target game_sidescroller
./build/game_sidescroller
```

The first line of stdout is the gate's machine half:

```
[game] materials: on (3 pipeline(s), 0 fallback(s))
```

`off` means the bundle was not found next to the binary — the install step copies both
`game.bundle` and `library.bundle`, so an `off` here after a clean build is a finding, not a
configuration to fix by hand. A non-zero fallback count means a material named an entry point the
shader does not carry: the game keeps drawing, which is the point of the fallback, and the number
is the evidence that it happened.

Then watch the screen and answer three questions:

1. **Enemies flash red as they close in.** The strength is the enemy's own progress across the
   screen, not a timer: an enemy that just entered from the right is barely tinted, one about to
   reach the hero is strongly red. A flash that pulses in lockstep across all enemies means the
   parameter is coming from somewhere shared instead of from the instance.
2. **The boss is ringed in red while it is healthy.** `outline_danger`, two pixels, around the
   silhouette and not around its bounding box.
3. **Below a quarter of its health the boss burns to ash instead.** The ring is replaced by
   `dissolve_ash`: holes eat the sprite, widening as the health falls, with a grey rim on their
   edge. The switch happens once, at 25%, and does not flicker back and forth on the boundary.

The offscreen path is deliberately material-free: `--demo` renders the same scene without the
library so the render goldens of spec #2 stay byte-identical, and gate 8 of this spec is exactly
that regression. Seeing no effects there is correct.

## 11. Gate 3 of #18 — hot-reload in front of a person

> **Open.** Everything about hot-reload that a machine can assert is asserted twice and headlessly:
> `material_hot_reload` proves the cache-level contract on every OS in CI (a valid edit rebuilds all
> three pipelines; a broken one is refused, counted, and leaves *the same pipeline objects* drawing),
> and `editor_shell --gate3` proves the panel half by pixels — the preview hash changes on a valid
> edit and is **byte-identical** after a broken one. What neither can answer is the only question the
> feature exists for: does the picture on a real screen change while you keep playing, and does a
> typo leave it alone instead of blanking it.

### The editor half

```sh
cmake --build build --target editor_shell
LIKENES_MATERIAL_LIB=engine/material/library ./build/editor_shell --gate3 gate3_work
```

Expected, and the run must exit `0`:

```
[editor] materials panel: gate 3 runs its own copy
[gpu] <adapter> | <backend> | BC: <yes|no>
[material] hot-reload: gate3_work/sprite_effects.wgsl -> 6 pipeline(s) total
  rejected: gate3_work/sprite_effects.wgsl:1:1: error: expected global item (…), found 'let'
gate 3: PASS (failures: 0)
```

`6 pipeline(s)` is the whole point of the first line: three before the edit, three rebuilt after it.
A run that prints `3` reloaded nothing, and a run whose `rejected:` line carries no `:line:col:` has
a diagnostic the panel can show but not click through.

Then the eye, with the window open:

```sh
LIKENES_MATERIAL_LIB=engine/material/library ./build/editor_shell
```

The **Shaders** window shows the library preview, the watch backend (`native` on all three desktops;
`polling` means the native watcher refused and the fallback took over — a finding worth reporting,
not a failure), and the pipeline/reload/reject counters. With the window on screen, edit
`engine/material/library/sprite_effects.wgsl` in any editor and save:

1. **A real edit** — change the last line of `fs_flash` to `return vec4f(1.0, 0.0, 1.0, 1.0);` —
   repaints the preview magenta within about a second, and `reload(s)` goes up by one. No restart,
   no flicker of the rest of the panel.
2. **A broken edit** — put `let x: f32 = ;` on the first line — leaves **the picture exactly as it
   was**, adds one to `rejected`, and prints the file, line and column in red under the counters.
   The preview must not blank, flash, or fall back to a flat colour: that is the whole claim.
3. **Fixing the file again** brings the preview back, and `rejected` stops growing. A panel that
   needs a restart here has kept the broken module somewhere it should not have.

### The game half

The same driver runs inside the sample game, which is where it matters:

```sh
cmake --build build --target game_sidescroller
LIKENES_FX_WGSL=engine/material/library/sprite_effects.wgsl ./build/game_sidescroller
```

Startup names the watch, and it is the line that says the feature is armed at all:

```
[game] materials: on (3 pipeline(s), 0 fallback(s))
[game] shader hot-reload: engine/material/library/sprite_effects.wgsl (native watch)
```

Without `LIKENES_FX_WGSL` the game prints no watch line and hot-reload is off — that is the shipping
configuration, not a defect. With it, repeat the two edits above **while enemies are on screen**: a
valid edit changes the flash on live enemies mid-run and prints `-> 6 pipeline(s) total`, a broken
one prints the diagnostic plus `rejected, previous library still drawing`, and the enemies keep
flashing the old way. The frame rate must not stumble on either: the rebuild happens between the
tick and the frame, and a visible hitch on a three-pipeline library is a finding.

## 12. Gate 7 of #18 — the lit frame on a real GPU

> **Open.** The machine half is green on all three runners and it is genuinely load-bearing: the
> pass switched off returns the material frame byte for byte, switched on it changes that frame, a
> light set of a DIFFERENT length gives a different picture, and both slot passes report a MIXED set
> — five materials whose texture slot names a normal map and two without one, four naming an
> occluder and three without one — so the count, the normals and the shadows really do come out of
> the tables and not out of the shader. The shadow assertions carry their own broken implementations:
> a march that returns 1 leaves the frame untouched and trips two of them, an INVERTED march trips
> "a shadow only darkens", and a softness read from a constant instead of the row trips the fixture
> pair that differs in that one line.
>
> What no runner can answer is where the light lands, and one class of defect proves the point
> exactly: flipping the sign of Y in the dome generator leaves `--selftest` fully green (the frame
> is different but just as stable, and every counter still reads 5/2/0) while the reference PNG goes
> red at `frac=0.129`. Lit from below is a picture question, and the picture is yours.

```sh
cmake --build build --target light_golden
./build/light_golden --golden engine/render/golden/lights_640x360.png
```

Expected, on the Metal machine the reference was baked on:

```
[gpu] <adapter> | Metal | BC: yes
  lights: 5 source(s) in the table, 5 uploaded
  frame: 3 draw call(s) into the albedo target
  normals: 5 mapped from the table, 2 flat, 0 naming an unknown asset
  occluders: 4 mapped from the table, 3 open, 0 naming an unknown asset
  cost: 1.631 ms/frame without the pass, 1.658 ms/frame with it (30 frames each)
  golden engine/render/golden/lights_640x360.png: mean=0.00000 max=0.00000 frac=0.00000
light-gpu: PASS
```

The `cost` line is a measurement, not an assertion: it is printed so the pass has a price on record,
and a threshold on it would be a statement about your machine rather than about the pass (the
physics gate learned that in §4). Both numbers on the reference machine sit at about 1.7 ms for a
640x360 frame, and the pass costs the difference between them — some 30 microseconds, and the price
is honest: the scene is drawn THREE times, once into albedo, once into the normal buffer and once
into the occlusion buffer. The alternative was extra render targets on the material pass, which
would have rewritten every fragment entry point of `sprite_effects.wgsl` and moved the closed gates
4-6 with it.

The `normals` and `occluders` lines are assertions, not measurements, and CI greps both whole: 5 of
the library's 7 materials declare `tex | normal | ... | 2` and 4 declare `tex | occlusion | ... | 3`,
and a pass that bound one map for everything would print `7 mapped, 0 flat` or `0 mapped, 7 flat`.
The two counts DISAGREE on purpose — 5/2 against 4/3: a pass that read the wrong slot name would
still find maps and still draw a frame, and nothing but this pair of numbers tells the two slots
apart. `0 naming an unknown asset` says every slot resolved to a map in the bank.

`max=0.00000` means the checked-in reference is bit for bit what your GPU just drew. If your numbers
came out non-zero, write yours to a scratch path and compare by eye instead of pointing `--update`
at the pinned file:

```sh
./build/light_golden --golden /tmp/mine-lit.png
```

Then open the PNG and answer eight questions — the half the numbers do not cover. The source of
truth for every one of them is [`engine/light/library/lights.txt`](../engine/light/library/lights.txt),
and reading it first is the point: the gate asks whether the picture agrees with the DATA.

Sprite normals now come from the material's texture slot (step B of the vertical). The bank ships
two maps: `sprite_normal` is a DOME — a hemisphere inscribed in the sprite's circle — and
`outline_normal` is a RIDGE, four vertical ribs across the sprite. Which sprite gets which is
decided by `library.mat` and nothing else. The scene lays materials out in columns, left to right in
declaration order, four sprites each: `flash`, `flash_red`, `flash_gold` (dome), `outline`,
`outline_danger` (ridge), then `dissolve` and `dissolve_ash` — the two RIGHTMOST columns, flat.

1. **Hold it next to §9's `materials_640x360.png` — the same scene, unlit.** Every sprite must come
   out dimmer, and dimmer by a DIFFERENT amount depending on where it sits. An evenly darkened copy
   of that picture means only the ambient term reached the frame and the five sources did not.
2. **Warm at the top left, cold at the top right.** `key` is orange at `-0.42, 0.28`, `fill` is blue
   at `0.44, 0.16`; the frame's x runs `-1.78..1.78` and its y runs `-1..1`, y UP. The top-left
   sprites must pick up the orange and the top-right ones the blue. Swapped is a sign flip in x,
   upside down is one in y — the defect class the input round hit twice.
3. **The near-black ground is lit only where a source is close.** Between the lower rows it picks up
   a faint green haze at the centre — that is `rim`, at `0.06, -0.46`, with a radius of 1.10. An
   evenly lit background means the falloff is not applied at all; a background that stays pure black
   everywhere means the pass never reached it.
4. **`sun` is an even wash on the flat columns, and shaped on the mapped ones.** It is directional
   with `dir = 0.35, -0.94`, and `dir` is where the light TRAVELS, so it arrives from the upper
   left. Over the two rightmost columns, which carry no normal map, every pixel has the same flat
   normal and the sun can only add a CONSTANT cool tint — a contribution that varies with position
   THERE means it is being attenuated like a point light. Over the mapped columns it must vary with
   the surface instead, and the two shapes differ: the domes take the wash across a round face, the
   ribbed columns break it into vertical bands.
5. **The domes are lit from the upper left, not the lower left.** This is the question `--selftest`
   cannot ask. Each `flash*` sprite is a hemisphere: its bright side must face the light that
   reaches it, and with `key` at the top left the highlight sits ABOVE the sprite's centre, the
   shaded crescent below. A dome bright underneath and dark on top is a flipped Y in
   [`engine/render/slot_textures.cpp`](../engine/render/slot_textures.cpp) — the map stores its
   normal in the LIGHTING basis (+Y up), and the flip belongs in the generator, in one place.
6. **The ribs run vertically and answer to left and right.** The `outline` columns must show four
   light-and-dark bands ACROSS the sprite, not up it. Horizontal banding means x and y were swapped
   somewhere between the generator and the pass; no banding at all means the ridge map never reached
   the outline material and the flat clear survived in its place.
7. **Shadows fall AWAY from the source, and only two materials cast them.** Step C gives the
   `occlusion` slot to `outline` (a solid disc, the whole sprite blocks) and to `dissolve` (a grate,
   vertical bars blocking every other one). Their columns must trail a dim streak on the side
   opposite each source that reaches them — orange-side streaks pointing right and down from `key`,
   blue-side ones pointing left from `fill`. A streak pointing TOWARDS a source is a sign flip in the
   march direction of [`shaders_light.cpp`](../engine/render/shaders_light.cpp); a sprite darkened
   evenly all over instead of trailing a streak means the occluder is shadowing ITSELF — the `here`
   term under the sample point was dropped. The two shapes must differ: the disc's shadow is a solid
   band, the grate's is striped, and identical shadows under two different materials mean the map
   came from somewhere other than that material's own row.
8. **`flash` casts nothing, `dissolve` is lit flat.** The three `flash*` columns carry a normal and
   NO occluder; the two `dissolve*` columns carry an occluder and NO normal. So the domes must be
   shaded and cast no streak, and the dissolve columns must trail a striped shadow while taking the
   light evenly across their own face. A flash column that casts, or a dissolve column that is
   shaded, means the two slots were read off one another — exactly what the mismatched 5/2 and 4/3
   counts exist to prevent, seen from the picture side.

**On Linux and Windows run `--selftest`, not `--golden`** — the same reason as §9: this comparison
judges the peak error and would be about the rasteriser, not the pass. What it does say there is
portable and worth having: the assertions above plus two runs on that adapter agreeing bit for
bit.

```sh
./build/light_golden --selftest
```

## 13. Gate 8 of #22 — the network frame cost (Linux and Windows)

The third frame-cost gate, and the first one whose frame contains a *rollback*. Sections 4 and 5
measure a physics step and a character tick; this one measures the step a networked peer actually
takes — advance the simulation, roll back and replay when a late input arrives, write the tick into
the `.replay` stream — and, separately, what the socket costs beside it.

**Two numbers, not one.** A single total says a frame was expensive without saying what made it so,
and the two halves fail for different reasons: `sim` grows when rollbacks deepen or the recording
does, `net` grows when the reliable layer resends. Summed, a 0.3 ms frame and a 0.3 ms frame are
indistinguishable, and the fix for each is in a different file.

**The binary is the one that records.** `--peer` writes the replay stream through `RecordingSim`,
a line per tick — measuring a build without the recording would describe a frame that does not exist
in vertical 2. This is why the gate runs the peers themselves and not
`game_platformer_net_test`: that harness sends its children's stdout to `/dev/null`, so the two
lines this gate exists for never reach the report.

```sh
cmake --build build --target game_platformer_net_test
bash scripts/owner_net_budget.sh
```

(Windows: `scripts\win-dev.bat check` builds the tree, then the same `bash scripts/owner_net_budget.sh`
from git-bash. A full `bash scripts/owner_check.sh` already carries this stage, right after the
physics and character one.)

Expected output. The counters on the first line of each peer are **not repeated here** for the
reason given in §5 — they are pinned by the scripted route and a copy in a runbook is a copy nothing
checks; a run that disagrees with them fails inside `game_platformer_net_test`, not here.

```
--- Цена кадра сетевого прогона (game_platformer_net_test --peer, гейт 8 спеки #22)
  peer send: ticks=417 rollbacks=<n> replayed=<n> forced=0 resent=<n>
  peer send: sim worst=<time> ms mean=<time> ms over 417 ticks
  peer send: net worst=<time> ms mean=<time> ms over <n> passes
  peer recv: ticks=417 rollbacks=<n> replayed=<n> forced=0 resent=<n>
  peer recv: sim worst=<time> ms mean=<time> ms over 417 ticks
  peer recv: net worst=<time> ms mean=<time> ms over <n> passes
  худший кадр пира recv: <time> мс из 16.67 (<n>% бюджета)
  худший кадр пира send: <time> мс из 16.67 (<n>% бюджета)
```

The M3 Pro reference, 2026-09-03 (macOS 26.5.2, Apple clang, Release, idle box): `send` worst
0.112 + 0.100 = **0.212 ms**, `recv` worst 0.208 + 0.115 = **0.323 ms** — 1.3% and 1.9% of a 16.67 ms
frame; means 0.017/0.009 and 0.016/0.007. As in §4 and §5, that figure describes the M3 Pro. **The
machine that decides is the slowest one you own**, and it decides on both OSes.

What to judge, in this order:

1. **A non-zero exit, ahead of every timing.** `FAIL: пиры вышли ненулём: send=<n> recv=<n>` means
   the run did not finish; `9` there is specifically *"its frame budget was never measured"* — the
   peer counted fewer samples than ticks, and any timing printed beside it is about a probe that did
   not fire, not about a fast frame. Same class as a missing counter in §5.
2. **`sim` `over 417 ticks` on both peers.** One sample per tick is the arithmetic the whole gate
   rests on; the peer asserts it itself and exits `9` when it does not hold, but the line is worth
   reading, because it is where you see that the route ran at all.
3. **The two `худший кадр` percentages against a real budget.** Both halves add inside one frame,
   which is why the script sums them per peer rather than quoting four numbers. Add the physics
   `heap: mean=` and the character `target: mean=` from §4 and §5 on top: a real game frame carries
   all of them.
4. **`recv` costing more than `send` is expected, not a finding.** The receiver is the one that
   rolls back — it learns the input late — so its `sim worst` is the deeper one, and its socket
   makes roughly twice the passes because it spins waiting for input the sender never waits for.
   The reverse ordering on your machine is worth reporting.
5. **`resent=` on the sender.** Non-zero is normal on a loopback under load — it is the reliable
   layer doing its job — but a number in the hundreds beside a large `net worst` is the two halves
   telling one story, and that story is the socket, not the simulation.

**Measure on an idle machine**, for exactly the reason spelled out in §5: nothing in this output
separates a loaded run from a quiet one, and a build in another window is enough to turn a 1.9%
frame into a 20% one.

## 14. Gate 9 of #22 — a live session (two machines)

> **Half of this gate became runnable on 2026-09-04, and the other half did not.** Until that day
> both peers hard-coded `pnet::ADDRESS_LOOPBACK` and found each other by writing a port number into
> a file next to each other — one filesystem, one machine, no way to name a neighbour. They now take
> the neighbour as an argument, so **the convergence half — two processes on two machines, one
> input, one state, over a wire with real latency — you can run today**. The other half, *does it
> feel like one game*, still cannot be asked: the peer is headless by design (invariant 2 of the
> spec: *the same host without a window*), so there is nothing to look at. What follows is the
> procedure for the half that exists; the drawing peer stays an engine debt, named at the end.

### What you need

Two machines on one network — any pair of Linux, Windows and macOS, mixed is better than matched —
**both built from the same commit**. Not a formality: the whole claim is that identical inputs give
identical state, and two different commits would answer a question nobody asked. Check with
`git rev-parse HEAD` on both.

Note each machine's address (`ip addr` on Linux, `ipconfig` on Windows, `ipconfig getifaddr en0` on
macOS) and let UDP through the firewall on **both** ports below — each machine listens on one and
sends to the other, so `7777` and `7778` are each open on one side. Windows will ask on the first
run, Linux may need `sudo ufw allow 7777/udp` on A and `sudo ufw allow 7778/udp` on B. Measure the
wire first, because the answer at the end is read against it:

```
ping -c 20 <the other machine>
```

### The run

Build the gate on both machines:

```
cmake --build build --target game_platformer_net_direct_test
```

Machine **A** owns the input (the sample has one hero, so one side plays it):

```
./build/game_platformer_net_direct_test --peer send example_ugly_game/assets/game.bundle live \
  --listen 7777 --at <B>:7778
```

Machine **B** replays what arrives:

```
./build/game_platformer_net_direct_test --peer recv example_ugly_game/assets/game.bundle live \
  --listen 7778 --at <A>:7777
```

On Windows, `scripts\win-dev.bat` owns the compiler environment; the peer command is the same with
`build\game_platformer_net_direct_test.exe`. Start B first — it waits, A talks first — and start
both within twenty seconds of each other: `PEER_DEADLINE_MS` is 20 s, and a peer that never hears
its neighbour exits `4` rather than hanging.

Both addresses are named on **both** sides on purpose. A peer that learned its neighbour from
whoever wrote first would hand its acknowledgement window to whoever won that race, on a port that
— to reach another machine at all — is open on every interface.

### Expected output

Each side prints three lines and writes `live-send.replay` / `live-recv.replay` beside itself:

```
  peer send: ticks=417 rollbacks=21 replayed=84 forced=0 resent=139 aliens=0
  peer send: sim worst=0.146 ms mean=0.019 ms over 417 ticks
  peer send: net worst=0.050 ms mean=0.011 ms over 487 passes
```

(That is a real pair, run over the loopback on an M3 Pro on 2026-09-04. `resent` is the sender's
number; the receiver's is normally `0`, because only the sender has input to repeat.)

`aliens=` is the last field and it is printed for this run only — it counts datagrams that arrived
from an address other than the neighbour named in `--at`, and were dropped for it. On the loopback
it is always `0`; on a machine with several interfaces (a VPN, `docker0`, two cards) it need not be,
because the neighbour may answer from an address other than the one you typed. That is why it is
worth a line here: a pair that dies at the deadline with `aliens` climbing is a routing problem, and
a pair that dies with `aliens=0` never heard anything at all.

### What to judge, in order

1. **Both sides exited zero and printed `ticks=417`.** A non-zero code is the answer, not a
   nuisance, and the peer prints a line naming which one before it leaves:

   | code | what it means |
   |---|---|
   | `2` | the level did not load — the bundle path is relative to the **current directory**, so run both peers from the repository root or spell the path out in full |
   | `3` | nobody was named: a typo in `--at`, only half the pair of arguments, or a word the parser did not understand (it prints the word) |
   | `4` | the deadline passed without the neighbour being heard — a firewall, nine times in ten; if `aliens` is climbing it is routing instead, see the note under the output above |
   | `5` | the result file could not be written — check the directory the prefix points into |
   | `6` | the socket is unusable: either the port you passed to `--listen` is already taken (the line says `port N did not open`), or the socket went bad mid-run |
   | `7` | the recording could not be written, same directory question as `5` |
   | `9` | the frame was never measured — an engine finding, report it |
2. **The two recordings are the same file.** This is the gate itself — two machines, one input, one
   state, byte for byte:
   `sha256sum live-send.replay` on Linux, `shasum -a 256` on macOS, `certutil -hashfile
   live-send.replay SHA256` on Windows. Compare A's `live-send.replay` with B's `live-recv.replay`.
   **They differ only if `forced` is non-zero on one of them** — see 3.
3. **`forced=` on both sides.** Zero means every tick's input arrived and the rollback converged on
   it. Non-zero means a peer stopped waiting for an input that never came, played the prediction and
   kept it — that is the finding. Report the number together with the `ping` figures: `forced > 0`
   at 20 ms is a defect, `forced > 0` at 200 ms is arithmetic (`HOLE_PATIENCE_MS` is 3 s, but
   `PEER_PREDICT` is four ticks — 66 ms of play).
4. **`rollbacks=` and `resent=` against the loopback.** On the loopback pair the sender rolls back
   about 21 times and resends roughly a hundred of its 417 frames (89–139 across seven runs on an
   idle M3 Pro — the spread is the machine's scheduler, not the protocol). Over a real wire both
   must grow, and how much they grow is the number this gate exists to bring back.
5. **`sim`/`net worst=` against 16.67 ms.** Same reading as §13, and it is worth taking here as
   well: the network line covers a socket that now carries real datagrams, not loopback ones.

### What is still missing, and why it is not your problem

The subjective half — *does it play* — needs a peer with a window, and there is none. The two
constants this gate exists to challenge are `PEER_PREDICT = 4` and `PEER_DEPTH = 8`
([`platformer_peer.hpp`](../example_ugly_game/platformer_peer.hpp)): four ticks of prediction is
66 ms, chosen against a loopback whose latency is zero. They are the network analogue of the 500
bodies spec #15 claimed and your run cut to 350 — numbers picked where the measurement was cheap,
waiting for the machine that decides. Steps 3 and 4 above are what can be said about them without a
picture; the picture is engine work, not yours.

## 15. Gates 1 and 3 of #20 — the engine package runs where it was not built

> **All three packages are runnable from one machine today.** Verticals 1–3 of spec #20 build the
> **macOS** package on the host, the **Linux** package in a container on that same host, and the
> **Windows** package in a CI job whose artifact the script downloads and verifies. What needs your
> machines is the other half of the claim: a package built here has to run **there**, on a box that
> never saw this source tree.

### The run (macOS)

```
bash scripts/release.sh --version v0.1.0
```

`--only host` says the same thing explicitly; without the flag the script packages the host OS
anyway, and naming it spells out that the refusal on a foreign OS cannot be reached by a typo.

Expected — the table, then the same numbers again on a second run of the same command:

```
release: like-nes-engine-v0.1.0-macos-arm64

package                                             size  files  sha256
like-nes-engine-v0.1.0-macos-arm64.tar.gz       <bytes>     11  <64 hex>

внутри: like-nes/bin/assetc like-nes/bin/editor_shell like-nes/bin/libwgpu_native.dylib …
манифест: release/v0.1.0/like-nes-engine-v0.1.0-macos-arm64.manifest
суммы:    release/v0.1.0/SHA256SUMS
```

The byte size is deliberately not written down here: it follows the toolchain that built the
package, and a number pinned once would go stale with the next Xcode update, turning this step
permanently red. The claim is that the **two runs agree with each other**, not that either agrees
with this page. (The headers are Latin because `printf` measures column width in bytes, and
Cyrillic ones shift the columns.)

The package lands in `release/v0.1.0/` together with its `.manifest` and `SHA256SUMS`.

### What to judge

Copy `release/v0.1.0/like-nes-engine-v0.1.0-macos-arm64.tar.gz` to a machine (or a fresh user
account) **without** this repository and without a toolchain, unpack it anywhere, and run both
binaries from the unpacked tree:

```
tar -xzf like-nes-engine-v0.1.0-macos-arm64.tar.gz
cat like-nes/version.txt
./like-nes/bin/assetc --synthetic /tmp/probe.bundle
./like-nes/bin/editor_shell
```

1. `version.txt` names the version you asked for, the commit it was built from and the target
   triple — three lines, no `unknown`.
2. `assetc` prints its baking line and writes the bundle. A dynamic-loader error instead
   (`Library not loaded: …libwgpu_native.dylib`) is the finding: the runtime travels **next to** the
   binaries and is found through `@executable_path`, so it failing here means the package is not
   self-contained on a machine that has no wgpu anywhere else.
3. `editor_shell` opens a window on a box with no build tree. This is the part no gate on a runner
   can assert: the editor needs a real GPU and a real display server.
4. `like-nes/licenses/` holds seven files, none of them empty (regression of spec #9).
5. Unpacking twice into two directories and comparing (`diff -r`) gives no differences.

Everything above except point 3 is also asserted mechanically by `bash scripts/check_release.sh`,
which **packages twice out of one build** (in its own `build-release` directory) and compares the
runs — contents file by file, the stamp, the archive's normalisation and the sums. What it cannot
ask is whether the thing runs on a machine that is not this one.

### The run (Linux package, built on macOS)

Needs docker or podman with a **live** daemon — Docker Desktop actually running. With neither, the
script exits 4 and prints what to install (`brew install --cask docker` or `brew install podman`);
that is a refusal, not a crash, and it is a different code from the 3 that means "this platform is
not built here".

```
bash scripts/release.sh --version v0.1.0 --only linux
```

```
release: контейнер docker, платформа linux/arm64, образ like-nes-release:33ceb71981b6-aarch64
release: база ubuntu@sha256:33ceb719...517
<docker build output; CACHED once the layer exists>
release: like-nes-engine-v0.1.0-linux-aarch64

package                                            size  files  sha256
like-nes-engine-v0.1.0-linux-aarch64.tar.gz     <bytes>     11  <64 hex>

внутри: like-nes/bin/assetc like-nes/bin/editor_shell like-nes/bin/libwgpu_native.so ...
release: пакет Linux собран: release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.tar.gz
```

The container runs **the same `release.sh`** — it supplies a Linux host, not a second packer — so
the table and the contents match the macOS run except for the runtime name (`libwgpu_native.so`).
The tree is mounted read-only and the build directory lives outside it, so `git status` is clean
afterwards. The base image is pinned by **digest**, not by the `ubuntu:24.04` tag: a tag moves to a
new image silently, and a package built by "the same command" would stop being the same package.

Then copy `release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.tar.gz` to a Linux box with no source
tree and no toolchain, and run the same four steps as above. Point 3 is where the clean distro's
X11/Wayland dependencies surface — that is gate 5 of this spec, and no runner can answer it.

`bash scripts/check_release_container.sh --live` asserts everything except points 2-3 **on a foreign
machine**: it builds the package in the container twice and compares composition, stamp, runtime
name, archive normalisation and sums across the two runs. Without `--live` the same gate checks only
the rules (digest pin, read-only mount, refusal codes) and needs no daemon — that is the form that
runs inside `scripts/preflight.sh`.

### The Windows package (from CI)

Needs `gh`, authenticated against this repository. No client or no login is refusal code 4 naming
which of the two to fix — "install it" and "log in" are different actions.

```
bash scripts/release.sh --version v0.1.0 --only windows
```

```
release: жду прогон release_engine.yml на коммите <12 hex> (артефакт like-nes-engine-windows)
release: прогон <id> зелёный (опросов <N>)
release-check: OK  архив доехал целым (сумма совпала с SHA256SUMS артефакта)
release-check: OK  цепочка сошлась: прогон на <commit>, тот же коммит в штампе, версия v0.1.0
release: пакет Windows приехал: release/v0.1.0/like-nes-engine-v0.1.0-windows-x86_64.tar.gz
```

The run needs the tag pushed: the run is located **by the commit the tag points at**, never by the
tag name — a tag is moved by one command, and a run found by it could have been built from another
tree. With no tag on HEAD, dispatch it by hand:
`bash scripts/release_ci.sh --dispatch --version v0.1.0-check`.

A manual dispatch only works once `release_engine.yml` has landed in `main`: GitHub reads
`workflow_dispatch` from the **default branch**, not from the branch the run is asked for. While the
file lives only in `dev`, `gh` answers `could not find any workflows named release_engine.yml` —
that is a statement about the branch, not about the machine. The orchestrator checks earlier still
that the local commit is on the remote, and refuses with «the run would build another tree» when it
is not.

Inside the job the **same `release.sh`** runs: CI supplies a Windows host, not a second packer. The
chain assertion is the whole point of the path — the run must be on our commit, the artifact must
come from that run, and the stamp inside the package must name that same commit. All three agree
and *that* package arrived; they disagree and *some* package arrived, and the difference is
everything a foreign machine costs. The sum is compared against the `SHA256SUMS` that travelled in
the same artifact, which is a claim about **delivery, not authenticity** — the artifact carries no
signature, and calling that check a defence would be a lie.

The package is then asserted the way a locally built one is — **composition by name, licences, stamp
with the triple** — because the chain says nothing about contents: a package with no
`editor_shell.exe`, no `wgpu_native.dll` or a zero-byte licence agreed on both stamp lines and
arrived as a success. The run itself asserts nothing either: it is `release.sh` and an upload.

A red run is a refusal with a link (`gh run view <id> --log-failed`), not a hang: green, red and
still-running are three outcomes, and a red one does not wait out the deadline.

Then copy `like-nes-engine-v0.1.0-windows-x86_64.tar.gz` to a Windows box with no source tree and no
MSVC:

```
tar -xzf like-nes-engine-v0.1.0-windows-x86_64.tar.gz
type like-nes\version.txt
like-nes\bin\assetc.exe --synthetic %TEMP%\probe.bundle
like-nes\bin\editor_shell.exe
```

1. `version.txt` names the version, the commit and the `windows-x86_64` triple.
2. `assetc` prints its bake line; a missing-DLL error is a finding — the wgpu runtime ships beside
   the binaries.
3. `editor_shell` opens a window. A demand for the Visual C++ Redistributable, if it appears, is
   gate 5 of this spec.
4. `like-nes\licenses\` holds seven files, none empty (regression of spec #9).

`bash scripts/check_release_ci.sh` asserts the rules of the path (what the job builds, that it
publishes nothing, how the run is picked) without touching the network — that is the form inside
`scripts/preflight.sh`. `--live` runs the whole orchestrator: dispatch, wait, download, chain.

## Beyond the gates

The gates above are what the ADRs waited on; all but the three of spec #18 and the two of spec #22
are closed — and of those two, §14 lost its blocker on 2026-09-04 and now waits only for a second
machine. A machine with a screen, speakers and a pad can
also exercise things no gate covers — playing the sample game long enough to hear the audio, the
achievement toast surviving a restart, the offscreen `--demo` render path, an output device yanked
mid-frame, and `assetc` reproducing `bundle_hash = 0x1a557ae839e76ea0` byte for byte on another OS.
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
- The platformer screen recording per OS, its startup line, and the seven answers from section 6.
- The three `golden` lines from section 8 per OS, with the GPU name from the `[gpu]` line above
  them — and `golden_actual.png` if `frame` came out red.
- For §9, the frame `material_golden` writes on the Metal box, with the four answers — and, from
  the Linux and Windows boxes, the `--selftest` output with the `[gpu]` line above it.
- For §11, the `editor_shell --gate3` output with its `[gpu]` line, plus one recording per half:
  the editor panel through the three edits (real → broken → fixed) and the game through the same two
  while enemies are on screen, with the startup `hot-reload:` line visible in the terminal.
- For §13, the full `owner_net_budget.sh` output per OS — the two `худший кадр` lines and the four
  timing lines above them, with a note that the box was idle. The counters must match the other
  machines; the timings must not, and the spread is the point.
- For §14, the six lines both peers printed, the `ping` figures between the two machines, and the
  two SHA-256 sums of `live-send.replay` and `live-recv.replay`. Send them even when everything
  matched: `forced=0` at a measured latency is the first number this engine has about its prediction
  horizon, and `forced > 0` is the finding.
- For §15, `like-nes/version.txt` from the unpacked package on the other machine, the `assetc`
  output there, and one line saying whether `editor_shell` opened a window — plus the two `sha256`
  numbers from the two `release.sh` runs, which must be the same. Send the same three for the Linux
  package on a Linux box (naming the system libraries `editor_shell` missed, if it did not open) and
  for the Windows package that came back from CI (naming whether the Visual C++ Redistributable was
  demanded).
- For gate 9 of #17, two pairs of recordings and their answer sheets: `game_platformer` before/after
  `2bdfcb7` with the five answers, and `game_sidescroller` before/after `ddd0efa` with the six.
  One pair without the other is still worth sending — the halves are independent.

Every one of those except §9 has been sent, and each ADR that was waiting on one is now *Accepted*:
[0013](../.context/decisions/2026-07-27-desktop-dev-parity.md) (2026-08-06),
[0014](../.context/decisions/2026-07-28-framework-input.md) (2026-08-07),
[0015](../.context/decisions/2026-08-08-physics-core.md) (2026-08-22),
[0016](../.context/decisions/2026-08-30-character-tilemap.md) and
[0017](../.context/decisions/2026-08-30-graphics-framework.md) (2026-09-02). The list stays as the
shape of a re-run: when a commit touches the surface named in the right-hand column of the table at
the top, this is what the re-run is expected to produce.

One thing on this page is still unanswered, and no gate waits on it: **AMD**. Section 8 has now been
compared against Metal, lavapipe, DX12-WARP, Intel/Mesa, Intel/Windows and NVIDIA/Vulkan — six
stacks, three of them real drivers, and not one AMD. It is the only adapter vendor nothing in this
project has ever run on.
