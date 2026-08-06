# Owner verification: the gates a runner cannot close

Three gates are still open, and all three need a machine a CI runner is not: a real desktop
session, a real GPU driver, a real gamepad.

| Gate | Spec | Where |
|---|---|---|
| Linux X11 **and** Wayland — editor renders, gizmo moves an object | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 6 | Linux |
| End-to-end: clone → build → editor → edit game code → change visible | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 8 | Linux **and** Windows |
| Live input: pad passport, profile, runtime rebind, unplug mid-session | [#14](../.context/specs/2026-07-26-framework-input.md) 8 | Linux **and** Windows |

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

It writes `build/owner-report-<os>.txt`: machine passport (OS, distro, compiler CMake actually used,
session type, Vulkan device, input nodes), the build gate, the workflow linter, every self-contained
test in the tree, and three timed runs of the edit→build→hot-reload loop. Tests that need paths to
plugins or bundles are skipped by name and say so — CI runs those with arguments on all three OS.

Green means only that this machine agrees with the runners. The three gates below are what the
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

```sh
cmake --build build --target framework_input_probe
./build/framework_input_probe example_ugly_game/assets/input.txt
```

Run it from the repo root: the manifest path is relative, and `input.txt` is the *source* preset —
the probe bakes it in-process, so there is no separate bake step. On Windows use
`scripts\win-dev.bat probe`: `cmake` is not on PATH there until vcvars has run, which is the same
reason `check` and `gate8` have wrappers. It rebuilds that one target (a pull leaves the old binary
in place silently) and runs it from the tree root whatever shell you started in.

The probe opens a small window (keyboard and mouse arrive through it — it must have focus) and
drives the native pad backend (XInput / evdev / GameController). Everything it knows it prints.

Two Linux-side traps before you start, both of which make a working pad look like a broken backend:

- **Permissions.** The evdev backend reads `/dev/input/event*`; a desktop session normally gets
  them through logind's `uaccess`, but a pad seen by `ls /dev/input` and not by the probe means
  exactly that seam. `sudo usermod -aG input $USER`, then log out and back in.
- **Steam.** On a gaming distro (Nobara ships Steam) a running Steam with Steam Input on
  re-presents the pad as a *virtual* Xbox 360 controller and hides the real one. The passport line
  then names the wrong device through no fault of ours. Quit Steam completely for this gate — the
  point is what the OS reports about the physical pad.

1. **Passport → profile.** Plug the pad in *while the probe runs*. It must print one
   `pad 0 CONNECTED vid=… pid=… name="…" -> profile '…'` line. Check the profile matches the
   device: an Xbox pad must not come up as `generic`. A pad that is genuinely unlisted *should*
   say `generic` — that is the fallback working, not a failure. Send this line as-is.
2. **Live resolution.** Push the left stick: `move=(…)` follows it and rests at exactly
   `(+0.00,+0.00)` when centred (that is the deadzone). Press the south button and space — `fire:#`
   lights for both.
   **Name the directions out loud, one axis at a time** — right must read `x > 0`, and **up must
   read `y > 0`**. This is not pedantry: the three backends disagree on the sign of the raw Y axis
   (evdev grows downwards, XInput and GameController upwards), so an inverted stick is a
   *per-platform* defect that a pad tested on one OS cannot reveal. The engine's contract is in
   `engine/input/codes.hpp` — +X right, +Y **down** for the raw axis — and the preset flips it once
   with `padaxis:-ly`, which is why `move` reads up as positive.
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

## Beyond the gates

The three gates above are what the ADRs wait on. A machine with a screen, speakers and a pad can
also exercise things no gate covers — playing the sample game long enough to hear the audio, the
achievement toast surviving a restart, the offscreen `--demo` render path, an output device yanked
mid-frame, and `assetc` reproducing `bundle_hash = 0xa67f56b681aed040` byte for byte on another OS.
Those scenarios, with the exact commands per platform, are sections A–F of
[`owner-setup.txt`](owner-setup.txt).

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
