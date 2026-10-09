# Owner verification: the gates a runner cannot close

**14 of the 28 gates below are closed**, and each closed one carries the run that closed it, with the
evidence. The other 14 stay here as the procedure, because each needs a machine a CI runner is not: a
real desktop session, a real GPU driver, a real gamepad, a second box on the same network, a box that
never built this tree.

Which gate is which is **not prose kept in sync by hand**. Every section below carries a
machine-readable `<!-- gate: open | what to do -->` or `<!-- gate: closed <date> -->` right under
its heading, the table below is checked against those marks, the closing banner of a closed one is
checked against its date, and `scripts/owner_check.sh` prints the open list — with the step, not
just the address — by reading them instead of holding a copy. That check is `scripts/check_owner_gates.sh`; before it
existed, the script called gate 9 of #17 open for four days after it closed, and never mentioned six
of the open ones at all. A gate is re-run when a commit touches what it covers; the right-hand column
names that surface.

| Gate | Spec | Where | Closed | Re-run when a commit touches |
|---|---|---|---|---|
| Linux X11 **and** Wayland — editor renders, gizmo moves an object | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 6 | Linux | 2026-08-05 | window backend, surface glue, `LINUX_WAYLAND` |
| End-to-end: clone → build → editor → edit game code → change visible | [#13](../.context/specs/2026-07-26-desktop-dev-parity.md) 8 | Linux **and** Windows | 2026-08-05/06 | build loop, watcher, hot-reload, `win-dev.bat` |
| Live input: pad passport, profile, runtime rebind, unplug mid-session | [#14](../.context/specs/2026-07-26-framework-input.md) 8 | Linux **and** Windows | 2026-08-07 | `engine/input` backends, presets, profiles |
| Physics frame cost against a real frame budget | [#15](../.context/specs/2026-07-26-physics-core.md) 8 | Linux **and** Windows | 2026-08-22 | `engine/framework/physics`, load scenes, solver iterations |
| A target-size level costs a small one's tick, and that tick fits a frame | [#16](../.context/specs/2026-07-26-character-tilemap.md) 7 | Linux **and** Windows | 2026-09-01 | `engine/framework/character`, `engine/framework/tilemap`, query window |
| The platformer sample plays: slope, one-way, moving platform, and it feels responsive | [#16](../.context/specs/2026-07-26-character-tilemap.md) 8 | **all three** | 2026-08-30, re-closed with artefacts 2026-09-01 | `engine/framework/character`, `engine/framework/tilemap`, `example_ugly_game/platformer_*` |
| The samples look the same after being moved onto the graphics framework | [#17](../.context/specs/2026-07-26-graphics-framework.md) 9 | **any one** | 2026-09-02 | `example_ugly_game/platformer_view.*`, `example_ugly_game/fx*`, `example_ugly_game/sprite_out.*`, `engine/framework/graphics` |
| The reference frame holds against a real GPU driver, not a software rasteriser | [#17](../.context/specs/2026-07-26-graphics-framework.md) 2 | **all three** (a real AMD/NVIDIA/Intel driver each) | — | `engine/render/*`, `example_ugly_game/golden/scene_960x540.png`, shader sources |
| The effect library draws all three effects, and they are what the material says | [#18](../.context/specs/2026-07-26-materials-shaders.md) 1 | **macOS** (the reference is pinned on Metal) | 2026-10-03 | `engine/material/library/*`, `engine/material/cache.cpp`, `engine/render/material_*` |
| The sample game plays with library materials, and the effects land where they should | [#18](../.context/specs/2026-07-26-materials-shaders.md) 9 | **any one** with a screen | 2026-10-03 | `engine/material/library/*`, `example_ugly_game/material_fx.*`, `example_ugly_game/assets/library.bundle` |
| A shader edit lands without a restart, and a broken one leaves the picture alone | [#18](../.context/specs/2026-07-26-materials-shaders.md) 3 | **any one** with a screen | 2026-10-03 | `engine/material/hot_reload.cpp`, `engine/material/reload.cpp`, `tools/ide/editor/material_panel*`, `example_ugly_game/material_fx.*` |
| Five lights out of a table light the scene, and the light is where the data says | [#18](../.context/specs/2026-07-26-materials-shaders.md) 7 | **macOS** (the reference is pinned on Metal) | — | `engine/light/*`, `engine/render/light_*`, `engine/render/shaders_light.cpp` |
| The network frame — rollback, recording and socket — fits a real frame budget | [#22](../.context/specs/2026-09-02-deterministic-net.md) 8 | **the slowest machine you own** | 2026-10-04 | `engine/net/*`, `engine/framework/rollback/*`, `example_ugly_game/platformer_peer*` |
| A live session between two machines: one input, one state over a real wire (windows still missing) | [#22](../.context/specs/2026-09-02-deterministic-net.md) 9 | **two machines on one network** | — | `engine/net/*`, `engine/framework/rollback/*`, `example_ugly_game/platformer_peer*` |
| An engine package built here runs on a box that never saw this source tree | [#20](../.context/specs/2026-07-26-release-installers.md) 1 | **all three**, each a box that never built this tree | — | `cmake/install_engine.cmake`, `scripts/release*.sh`, `packaging/` |
| The forms people actually install from — `.dmg`, `.AppImage`, `.msi` — install and start | [#20](../.context/specs/2026-07-26-release-installers.md) 3 | **all three** | — | `scripts/release_dmg*`, `scripts/release_appimage*`, `packaging/like-nes.wxs.in` |
| No Visual C++ Redistributable on the box, and a silent install still lands | [#20](../.context/specs/2026-07-26-release-installers.md) 4 | **Windows** | — | `cmake/msvc_runtime.cmake`, `cmake/msvc_redist.cmake`, `packaging/like-nes.wxs.in` |
| The install page followed by someone who did not write it | [#19](../.context/specs/2026-07-26-docs-en-ru.md) 5 | **macOS and Windows** (the container answered for Linux) | — | `docs/en/getting-started/**`, `docs/ru/getting-started/**` |
| Level 1 survives a save in a real Tiled and bakes to the same bytes | [#24](../.context/specs/2026-10-01-content-pipeline.md) 3 | **any one** with Tiled 1.10+ | 2026-10-03/04 | `games/neon-rumble/levels/*`, `engine/framework/tilemap/tiled/*`, `tools/assetc/level_source.*` |
| Boxes and an event drawn in a real Aseprite 1.3 reach the clip table | [#24](../.context/specs/2026-10-01-content-pipeline.md) 3 | **any one** with Aseprite 1.3+ | — | `engine/framework/graphics/aseprite_*`, `engine/framework/graphics/clip_*`, `tools/assetc/bakers_clips.cpp` |
| The street of Neon Rumble on a real screen: parallax, signs, 21:9 and 4:3, the credits, a fresh Tiled save | [#24](../.context/specs/2026-10-01-content-pipeline.md) 7 | **any one** with Tiled 1.10+ | 2026-10-07 | `games/neon-rumble/levels/*`, `games/neon-rumble/src/rumble_credits.*`, `engine/framework/graphics/viewport_fit.*`, `engine/framework/graphics/text_*` |
| Three Puffolotti fighters walk level 1 in depth, the nearer one on top, and their style sits in the street | [#25](../.context/specs/2026-10-05-brawl-framework.md) 1 | **any one** with a screen and a keyboard | 2026-10-06 | `games/neon-rumble/src/rumble_brawl*`, `games/neon-rumble/src/rumble_keys.*`, `games/neon-rumble/src/rumble_roster_quads.*`, `engine/framework/brawl/*` |
| Banderas jabs, kicks and jump-kicks the dummy Adler, strikes pass through the ally Rainbird, F3 shows the hit frames and the depth bands | [#25](../.context/specs/2026-10-05-brawl-framework.md) 2 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/src/rumble_kinds.*`, `games/neon-rumble/src/rumble_brawl*`, `games/neon-rumble/src/rumble_depth_overlay.*`, `engine/framework/brawl/hit_*` |
| Adler, jump-kicked, falls, lies and gets up with his feet on his shadow; nothing hits him while he is down or rising | [#25](../.context/specs/2026-10-05-brawl-framework.md) 3 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/assets/puffolotti/*.json`, `engine/framework/brawl/body_react.*`, `engine/framework/brawl/body_clip.*` |
| Banderas's J chains jab, jab and cross on a hit and starts over on a miss; a key pressed during a strike waits for it | [#25](../.context/specs/2026-10-05-brawl-framework.md) 4 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/assets/puffolotti/*.json`, `engine/framework/brawl/body_chain.*`, `engine/framework/brawl/strike_queue.*`, `engine/framework/brawl/archetype_chain.*`, `engine/framework/brawl/fighter_chain.*`, `engine/framework/brawl/brawl_step.cpp`, `engine/framework/brawl/body_clip.*` |
| Banderas runs on a double tap, L on the run is a kick that rolls on into Adler; the run ends on release, reversal, strike or hurt | [#25](../.context/specs/2026-10-05-brawl-framework.md) 5 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/src/rumble_brawl.cpp`, `engine/framework/brawl/body_run.*`, `engine/framework/brawl/run_state.hpp`, `engine/framework/brawl/fighter_move_parse.*`, `engine/framework/brawl/brawl_step.cpp`, `engine/framework/brawl/body_clip.*` |
| Banderas holds a block on I and leaves it on the release; O rolls him forward through Adler, and nothing stops the roll | [#25](../.context/specs/2026-10-05-brawl-framework.md) 6 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/src/rumble_keys.*`, `engine/framework/brawl/body_guard.*`, `engine/framework/brawl/body_react.*`, `engine/framework/brawl/hit_apply.cpp`, `engine/framework/brawl/brawl_step.cpp`, `engine/framework/brawl/body_clip.*` |
| Banderas grabs Adler on H and throws him; Adler flies, lies and rises, and the jump kick hits him lying | [#25](../.context/specs/2026-10-05-brawl-framework.md) 7 | **any one** with a screen and a keyboard | — | `games/neon-rumble/fighters/*`, `games/neon-rumble/src/rumble_keys.*`, `engine/framework/brawl/hit_grab.*`, `engine/framework/brawl/hit_collect.cpp`, `engine/framework/brawl/hit_apply.cpp`, `engine/framework/brawl/body_react.*`, `engine/framework/brawl/body_clip.*` |

The last to close was the street of Neon Rumble (§20), on 2026-10-07 on macOS: the window was run
the day before, and what it lacked was the save itself, now on record with its mtimes and an empty
diff. Before it, the first gate of #25 (§21) closed on 2026-10-06 on macOS: the first run put the
fighters in the air in front of the facades, and the second, on the fixed build, put their feet on
the pavement. Before it, the network frame cost (§13) closed on 2026-10-04 on the Windows box — the slowest
machine in the set on both OSes, which is exactly what that gate asks for. Its Linux half was taken
the day before, and the two halves disagree about which peer is the expensive one: the asymmetry the
gate rests on is inverted here, and the receiver's rollback count turned out not to be pinned by the
scripted route at all. Before it, level 1 out of a real Tiled (§18) closed on 2026-10-03/04 on that
same box: the Linux run of 2026-10-03 could settle the bake and the numbers but had no Tiled on it at
all, and the save, the decoded diff and the window held against the Tiled view are what the Windows
box added the next morning. Before them, three gates of #18 closed on 2026-10-03 across two
machines. Two went on
the Metal machine the references are pinned on: the effect library's own frame (§9) and the sample
game playing with it (§10); the same pass left §12 open with a finding in its question 7, and left
hot-reload (§11) standing on one look alone — whether a reload costs a frame or blinks the panel.
That look is a transient, which no still can hold, so it was taken on the Nobara box by sampling the
window twenty times a frame: it does neither, and §11 closed the same day. Before them, the last to close was the look of
the sample after the framework move, and it is the kind a runner
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

## 1. Gate 6 of #13 — X11 and Wayland (Linux)

<!-- gate: closed 2026-08-05 -->

> **Closed 2026-08-05** on Nobara 44 (Intel UHD 620 / Vulkan): Wayland under GNOME and X11 under i3,
> one run each, both PASS. Kept as the procedure — it is what a new machine or a change to the
> windowing path has to be re-run against.

> **Re-run of the Wayland half, 2026-10-03**, on the same Nobara box, because the windowing path did
> change: 29 commits touched `engine/platform`, `engine/gfx`, `tools/ide/editor` and the root
> `CMakeLists.txt` since the closing date, among them the surface seam (`db0156c`), the Wayland
> choice passed on to the glue (`4429658`) and the build-dir/session mismatch message (`4b91548`).
> `build-way` configured and built clean against the three devel packages named below;
> `./build-way/editor_shell --gate6 …` came back **PASS, failures: 0**, with `glfw: wayland` on the
> passport — a native Wayland client, not XWayland. Evidence:
> `build/owner-artifacts-linux/g1-gate6-wayland.{txt,png}`; the PNG is a live viewport (grid, the
> gizmo on `entity_0`, the Inspector showing `x [fix32] = -8363008`, console `undo depth: 1`), not a
> flat fill. The gate stays closed on its 2026-08-05 date — this is a confirmation, not a re-close.
>
> The two eye-questions above stayed with the owner, and on this box **no AI can judge them**: GNOME
> here denies `org.gnome.Shell.Screenshot` (`AccessDenied: Screenshot is not allowed`) and the box
> has no `grim`/`gnome-screenshot`, so a native Wayland window's pixels cannot be captured from
> outside the process at all; and Wayland gives no client a way to inject input into another, so a
> *real* mouse drag of the gizmo is literally a hand's job. The X11 half of this re-run is not done:
> Fedora 44 dropped `gnome-session-xsession` from the repos, and picking the installed `i3` session
> means logging out — an owner action.

> **Re-run of the X11 half, 2026-10-03**, same box, i3 on Xorg, commit `a8e67c0`:
> `./build/editor_shell --gate6` came back **PASS, failures: 0** with `XDG_SESSION_TYPE=x11`,
> `glfw: x11`, `DISPLAY=set WAYLAND_DISPLAY=unset`. Evidence: `build/owner-artifacts-linux/x11/`.
>
> * **The first run passed with the gizmo off screen.** i3 tiles a new window, so it came up
>   1916x507; the camera keeps its zoom and centres the scene, and the top row of the grid —
>   `entity_0` and its gizmo — fell above the viewport's edge (`gate6-x11.png`). Every check stayed
>   green, because the hit-test checks are arithmetic on `world_to_screen` and never ask whether the
>   point lies inside the viewport. Run fullscreen (`$mod+f`) instead: `gate6-x11-full.png`
>   (1920x1080) shows the gizmo, and `screen-x11-gate6.png` — a grab of the X root window, i.e. the
>   screen itself, with no compositor in between — shows the same window on screen.
> * **The gizmo does not obey a mouse, real or synthetic, because the editor has no code for it.**
>   An XTEST drag along the X axis (`780,227 → 972,227`, 24 steps, button 1 held) left the
>   Inspector at `x [fix32] = -8388608` before and after, and `Ctrl+Z` changed nothing
>   (`mouse-strip.png`). The reason is in the tree, not in the session: `viewport_panel`
>   (`tools/ide/editor/editor_ui.hpp`) draws the axes and never reads the mouse; `gizmo_hit` is
>   called only from `editor_selftest.cpp` and `editor_gate6.cpp`, and the gate's "drag" is
>   `bus.set_component` called directly. `git log --all -S` finds no `IsMouseDragging`,
>   `IsMouseClicked`, `IsMouseDown` or `GetMouseDragDelta` in the whole history — the hit-test has
>   not been wired to input since it was added in `3aacd7a` (2026-07-21). The closing note's
>   "the gizmo obeys a real mouse" cannot have been observed; what a mouse does move is the
>   Inspector's `Position.raw` drag field. The gate stays on its 2026-08-05 date pending the owner's
>   decision: this is the question the gate exists to ask, and the answer today is **no**.
> * **GLFW 3.4 hangs on X11 when a window is created under a fullscreen window.** With the editor
>   fullscreen on the same i3 workspace, `neon_rumble --frames 120` never shows a frame: 1189 s and
>   counting at 99% CPU on one thread, stack `glfwCreateWindow → _glfwCreateWindowX11 →
>   waitForVisibilityNotify → XCheckTypedWindowEvent`; reproduced 2 of 2 under `timeout 20`
>   (`n9-x11.txt`). `waitForX11Event` returns true while *any* event is queued, so an event that
>   `XCheckTypedWindowEvent` does not take keeps the loop spinning and the 0.1 s timeout is never
>   spent. Upstream master has the same two functions unchanged. It affects every GLFW window of
>   the engine, editor included.
> * **Finding Н9 (a hidden window presents at 1 Hz) does not happen on Xorg.** Same binary,
>   `--frames 120`: alone on screen 2.00 s, moved to a hidden i3 workspace (unmapped) 2.00 s;
>   `--frames 600` covered by a fullscreen window *after* it was created 11.1 s. The 1 Hz is
>   mutter's, not the engine's — on this session the loop runs at refresh whatever the window's
>   visibility.

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
with, whether `DISPLAY`/`WAYLAND_DISPLAY` are `set` or `unset` — never their values, since this output
goes into a public PR — the adapter), drives select → gizmo hit-test → edit → Inspector →
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

<!-- gate: closed 2026-08-05/06 -->

> **Closed 2026-08-05/06** on commit `0e4294c`: Linux (Nobara 44, GCC) and Windows (MSVC 14.44),
> both PASS, dR=+89.705 against a +4 threshold, sim-golden intact. Kept as the procedure.

> **Re-run of the Linux half, 2026-10-03** on commit `a0f5894`, same Nobara box, gcc 16.2.1 — and
> the first attempt came back **FAIL (failures: 2)**, on the script rather than on the engine. The
> clear colour had left `draw.cpp` on 2026-08-31 (`b1dd1c7` folded the two copies of `begin_clear`
> into `batch.cpp` and made the colour the caller's argument), so step 4 patched a file that no
> longer holds it: `dR=+0.000 dG=+0.000 dB=+0.000`, two identical frames. **The gate caught its own
> rot** because step 4 asserts that the edit *applied* and not merely that it was asked for — the
> run without that assertion would have printed PASS over an unchanged frame for a month. Fixed in
> the same commit as this note: the patch now targets `example_ugly_game/demo.cpp` (the `--demo`
> offscreen path the gate actually renders), the "is the constant still where we think it is" check
> moved **ahead of** the build — a cold clone build is a quarter of an hour on this machine, and
> learning of a miss at the end of it costs half an hour for one answer — and the by-hand route in
> `docs/owner-setup.txt` point H now names `live.cpp`, which is the copy the window draws.
>
> The re-run on the fix is **PASS, failures: 0**, and it reproduces the closing numbers of
> 2026-08-05/06 to the third decimal, a month and a compiler version later:
> `7.466 7.552 23.086` → `97.171 7.552 15.285`, `dR=+89.705 dG=+0.000 dB=-7.801` against the +4
> threshold, sim-golden `0x32a094e89eacf2f2` unmoved in the same run. Evidence:
> `build/owner-artifacts-linux/g2-gate8-report-linux.txt` and `g2-gate8-e2e.log`, with the failing
> first attempt kept beside them as `g2-gate8-e2e-stale.log`. The gate keeps its 2026-08-05/06 date.

The editor has no Play/Build buttons yet: spawning and the build loop are their own targets
(`play_spawn_test`, `build_loop_test`), and the mechanism is what `owner_check.sh` already timed on
this machine. What is left is the chain end to end — an edit reaching pixels — and that whole chain
is one command:

```sh
bash scripts/gate8_e2e.sh
```

It clones this repository into `build/gate8/clone`, builds it from scratch, renders frames, patches
the clear colour in `example_ugly_game/demo.cpp` (`{0.02, 0.02, 0.07}` → `{0.25, 0.02, 0.05}`),
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

<!-- gate: closed 2026-08-07 -->

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
>
> **Re-run 2026-10-03 on Linux (Nobara 44), steps 3 and 4 only, PASS** — on commit `21fddf8`,
> because 9 commits touched `engine/input` and `engine/framework/input` since the 2026-08-29 re-run,
> among them the bounded cursors and the pair naming another axis (`f94ab5c`), the caps on the
> preset section (`95469d1`), counting logical axes once for baker, reader and runtime (`6f08e44`)
> and the section reader's own gate (`feef73b`). Steps 3 and 4 are the keyboard half of this gate
> and need no pad, so they were driven from a script — `XSendEvent` straight into the probe's
> window, because XTEST does not reach XWayland clients on this box. Step 3 refused by name on the
> first try, with the preset's own wording: `[probe] source already bound to 'jump' slot 0 - F takes
> it, C cancels`, and `F` then took it — the table printed `fire [0]=key:j` against `jump [0]=none`,
> which is `jump` losing that slot. Step 4 saved two edits (`bind | jump | 0 | none`, `bind | fire |
> 0 | key:j`), the restart read them back as `overlay loaded … 2 edit(s)` — the same count the
> 2026-08-07 closure reported — and `X` then `S` ended on `bye - overlay empty` with the preset
> restored to `key:space` / `key:j`. Evidence `build/owner-artifacts-linux/g3-steps34.txt`.
>
> **Steps 1, 2, 5 and the stick half of step 6 are not re-run: there is no pad on this box.**
> `/dev/input` holds no `js*` and no `by-id` directory, and the probe's own cold-start line says so
> out loud — `cold-start scan: backend 'evdev (Linux)' reports NO pad on any of 8 slots` — with the
> session's axis report ending in `stick pushed: right=NO left=NO up=NO down=NO`. Those four steps
> are a hand on a stick, and `padaxis:-ly` is exactly the kind of per-platform sign no keyboard can
> answer for. They stay open for the owner's next pass with the pad connected.
>
> **This re-run produced one finding of its own, in the gate's precondition** (finding Н10 of the
> 2026-10-03 Linux run, fixed in the same commit as this note). The box was carrying
> `~/.local/share/like-nes/controls_probe.txt` from the 2026-08-07 closure — fifteen bytes, zero
> edits, written by *this gate's own* `X` + `S` cleanup, which saves an empty overlay rather than
> deleting the file. The probe called it `overlay loaded … 0 edit(s)`, and the precondition block
> below reads `overlay loaded` as "the previous run was never cleaned up: delete the file". So the
> procedure rejected the state its own last step leaves behind, and the owner following it would
> delete a file for no reason — while the condition the trap actually hunts, an overlay holding
> *stripped bindings*, prints the same first three words. The probe now says `overlay at <path>
> holds no edits - clean preset` for that case, and only a file with edits in it is called loaded;
> all three branches were exercised on this box (`g3-overlay-three.txt`). The gate stays closed on
> its 2026-08-07 date.

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
Read it before step 1, and it must end in `clean preset`, which it does in two shapes:

```
[probe] no overlay for preset 'probe' at <path> - clean preset
[probe] overlay at <path> holds no edits - clean preset
```

The second shape is the ordinary state after a previous run was cleaned up properly: the `X` then
`S` at the end of step 4 **writes a preset with no edits, it does not delete the file**. Until
2026-10-03 the probe called that file "loaded" like any other, so this gate's own precondition
rejected the state its own cleanup leaves behind — the Linux box was carrying such a file from the
2026-08-07 closure, fifteen bytes and zero edits, and the line read `overlay loaded … 0 edit(s)`.
Now only a file with edits in it is called loaded.

`overlay loaded … N edit(s)` with N above zero means the previous run was never cleaned up: quit,
delete the file the line names (the path is *in* the line, so there is nothing to look up per OS),
start again. The `X` then `S` cleanup at the end of step 4 stays where it is — this is the check for
the run where it was forgotten, and a gate whose precondition is only a habit is not a gate.

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
   fire works on the south button. `-` and `=` step the master volume by a tenth and print
   `[game] volume <N>/10`: silent at 0/10, the launch level at 10/10, never louder than that. If the whole mix dips briefly in a dense fight or on the boss's death and comes back within about a tenth of a second, that is the output limiter (audit #21 A-3-1), not a finding; a crackle, a torn explosion or a bass that seems to breathe is. The game reads the same preset from the bundle — this is the
   "sample game on presets" half of the gate. If the stick moves nothing here but did move `move=`
   in the probe, say so: the two read the same axes through different preset tables, and that split
   is the whole diagnosis.

## 4. Gate 8 of #15 — the physics frame cost (Linux and Windows)

<!-- gate: closed 2026-08-22 -->

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

<!-- gate: closed 2026-09-01 -->

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

<!-- gate: closed 2026-08-30 -->

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
>
> **Re-run of the Linux half, 2026-10-03** on commit `6f6a798`, the same Nobara box, because the
> module underneath did move: 24 commits touched `engine/framework/character`,
> `engine/framework/tilemap` and `example_ugly_game/platformer_*` since 2026-09-01 — among them the
> surface seam every window loop now acquires its frame through (`db0156c`), the header prefixes
> (`d91546e`) and the Tiled import that rewrote the tilemap side (`591cce4`). **The mechanical half
> came back green, all five:** `game_platformer_sim_test` prints the route hash
> `0xfead7a87477a9258` unmoved, with both numbers the 2026-09-01 findings pinned reproduced to the
> digit (`edge: hero=24.125 leftmost=8.000`, `rider: hero=583.542 plate=555.999 slipped=1
> crushed=0`), plus `game_platformer_view_test`, `game_platformer_input_test`,
> `framework_character_push_test` and `framework_character_platform_test` — evidence
> `build/owner-artifacts-linux/g6-headless.txt`. The live window opens and closes as the gate says
> it must: `[gpu] Intel(R) UHD Graphics 620 (KBL GT2) | Vulkan | BC: yes`, then the one startup line
> with `gamepad: evdev (Linux)`, then `[platformer] window clean exit`, exit code 0, nothing on
> stderr — `build/owner-artifacts-linux/g6-live-passport.txt`.
>
> **Questions 1-7 stay with the owner: on this box no AI can judge them,** and the measurement that
> says so is in `build/owner-artifacts-linux/g6-input-probe.txt`. The input chain was proved alive
> first, so that "the sample ignores the keys" could be ruled out rather than guessed: with a
> temporary printf in front of `step_stage`, keys injected by `XSendEvent` straight into the window
> id arrive as the right intent — `d` and `Right` give `mx=1.000`, space gives `j=1`. What does not
> arrive is time: the loop runs **1.00 tick/s against the 60 it asks for** (14 ticks in 14.0 s of
> idle), because the surface is on `Fifo` and the world step sits in the same iteration as
> `win.draw`, so the sim advances at the pace the compositor presents. A window nobody can see gets
> presented once a second here, and three seconds of a held key buy two or three ticks of world —
> which is the whole of the "frozen hero" the pixel probes were reading. Raising the window to fix
> that is not available either: `wmctrl -i -a`, `-i -R`, `-i -r -b add,above` and `XRaiseWindow` all
> leave `_NET_ACTIVE_WINDOW` on XWayland's focus proxy `0x400003`, and GNOME denies
> `org.gnome.Shell.Screenshot` with no `grim`/`gnome-screenshot` on the box. Seven questions about
> whether a jump *feels* right need 60 Hz on a screen and a hand on a key, exactly as the banner
> above says. The gate stays closed on its 2026-08-30 date — this is a confirmation of everything
> mechanical, not a re-close. The 1 Hz itself is written up as finding Н9 of this run: the clock
> comment in `platformer_live.cpp` guards against a display that is too *fast* and says nothing
> about a present that blocks.

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
`surface texture status <n> - frame skipped` means the surface went stale after a resize or a
display change and was rebuilt (printed once; the picture must come back). `surface texture status
<n> - quitting` — with or without `, device lost` — and `[gpu] device lost (reason <n>): …` mean
the surface or the GPU is gone for good: the window closes by itself, the exit code is 1, and there
is no `window clean exit` line.

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

<!-- gate: closed 2026-09-02 -->

> **Closed 2026-09-02** on the Windows box — both halves in one pass, the records at the foot of
> each half below. Vertical 3 moved both samples onto `engine/framework/graphics`: step A took the
> platformer's camera, view window, tile drawing and draw order, step B3 took the shooter's
> particles and the instance buffer under them. Everything mechanical about either move is pinned
> headless — `game_platformer_view_test` and the untouched route hash `0xfead7a87477a9258` (gate 10)
> for the first, `game_fx_test` and `game_sprite_out_test` for the second — and what no runner can
> say is whether the picture on the screen is the same picture. That is two recordings and your
> eyes, **twice**: the two halves below are separate runs against separate "before" commits, and
> either can be answered without the other.
>
> **Re-run of the Linux half, 2026-10-03** on commit `7902b20`, Nobara, because 6 commits touched
> `engine/framework/graphics` since the closing date — the clip runtime and the F3 boxes
> (`925c48b`), the Aseprite bake into the clip table (`6f8b070`), the Tiled layers at runtime
> (`0e45039`), the subsystem header prefixes (`d91546e`), the four section readers checking their
> base (`08a69c1`) and the FNV constants moving into the primitives (`b4b10ae`).
>
> **The gate's own positive control passes on this box, and it is the first run to use it:** in the
> checkout `./build/game_sidescroller --golden-selftest` answers `golden control: PASS (blot,
> scatter and one-pixel shift refused)` and `golden repeat: PASS` and gives the prompt back, while
> the same command in `../like-nes-before-shooter` opens a window and starts the game, ignoring the
> flag it does not know — so the two trees are provably two trees, not one binary run twice. The
> weaker check agrees: 4 580 432 against 4 314 872 bytes, different md5. The old build also prints
> its bundle path unredacted (`/home/petrk/_dev/like-nes-before-shooter/...`), which is `6e6f3bf`
> landing after `ddd0efa`, not a finding. Evidence `build/owner-artifacts-linux/g7-control.txt`.
>
> **Everything the gate pins headless is green here:** `game_fx_test` with the documented numbers to
> the digit (`alive: 53, peak: 113, dropped: 0`, hash `0xfd9ca7d2936ad48a`), `game_sprite_out_test`
> (32 frames of 64 instances, 0 allocations), `framework_graphics_particle_refusal_test`,
> `game_platformer_view_test` and the route hash `0xfead7a87477a9258` — see
> `g7-headless.txt` and `g6-headless.txt`. The two startup lines the platformer half demands be
> identical **are identical**, before and after, and both runs end on `[platformer] window clean
> exit` with nothing on stderr and exit code 0: `g7-platformer-lines.txt`. That closes the one check
> this half puts before the frames ("a difference in either line is a finding before you look at a
> single frame").
>
> **The eleven picture questions — five for the platformer, six for the shooter — cannot be judged by
> any AI on this box,** for the measured reason written up in section 6 and in finding Н9: a window the
> compositor does not show is presented once a second here, so the loop runs 1.00 tick/s instead of
> 60; GNOME denies `org.gnome.Shell.Screenshot` and the box has no `grim`/`gnome-screenshot`; and no
> X11 call can raise an XWayland window to make its pixels trustworthy. Side-by-side recordings of
> two builds need a screen and two eyes. The gate stays closed on its 2026-09-02 date — this is a
> confirmation of the mechanical half plus the first live exercise of the positive control, not a
> re-close.

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

<!-- gate: open | эталонный кадр на живом драйвере: shooter, кадр 239 против scene_960x540.png — остался AMD -->

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
>
> **The NVIDIA answer cannot be had from Linux on this box, and the reason is named by the kernel,
> 2026-10-03.** The MX150 is in the machine — `01:00.0 3D controller: NVIDIA Corporation GP108M
> [GeForce MX150]` — but `Kernel driver in use` is blank for it, and Vulkan enumerates exactly two
> devices, `Intel(R) UHD Graphics 620 (KBL GT2)` and `llvmpipe`. The driver userspace *is* installed
> (`nvidia-driver-595.91.07-3.fc44`), and a module was built and did try to bind:
>
>     NVRM: The NVIDIA GPU 0000:01:00.0 (PCI ID: 10de:1d12)
>     NVRM: installed in this system is not supported by open
>     NVRM: nvidia.ko because it does not include the required GPU
>     NVRM: System Processor (GSP).
>     nvidia 0000:01:00.0: probe with driver nvidia failed with error -1
>
> The module akmods built is the **open** flavour (`modinfo … license: Dual MIT/GPL`), and the open
> module needs GSP, which arrived with Turing; the MX150 is GP108, Pascal. Nouveau would take the
> card, but `/usr/lib/modprobe.d/nvidia.conf` blacklists it, so nothing claims the GPU at all.
> Getting it back is the proprietary kernel-module flavour or an unblacklisted nouveau, each a root
> action plus a reboot — the owner's call, not this run's:
>
>     sudo dnf install -y akmod-nvidia
>
> (`akmod-nvidia` / `kmod-nvidia` 3:595.99.02-4.fc44 are in `nobara-nvidia-production`; which
> flavour that builds has to be checked against the same NVRM line afterwards.) **This changes
> nothing about the gate's standing**: the MX150 was answered on 2026-09-01 from Windows, on its
> own driver stack, and what stays open is AMD. Evidence:
> `build/owner-artifacts-linux/g8-nvidia-linux.txt`.

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

<!-- gate: closed 2026-10-03 -->

> **Closed 2026-10-03** on a MacBook with an M3 Pro, the Metal machine the reference is pinned on.
> The numbers came out bit for bit: `[gpu] Apple M3 Pro | Metal | BC: yes`, `warm-up: 3 pipeline(s)
> for 7 material(s), 0 fallback(s)`, `frame: 28 instance(s) in 3 draw call(s)`, `painted: 30.4% of
> the frame`, `mean=0.00000 max=0.00000 frac=0.00000`, `material-gpu: PASS`. The `fs_nope` fallback
> line is there; it prints after `painted` rather than first, which is two streams interleaving,
> not a finding. All four picture questions come back yes:
>
> 1. Seven columns and four rows.
> 2. White, red and gold ramp upward, and the bottom `flash_gold` sprite is already half-tinted.
> 3. The ring closes all round, outside the silhouette. It is black in `outline` and red in
>    `outline_danger` (2 px at the bottom), and it grows a pixel per row.
> 4. The holes eat the sprite upward. The rim is orange in `dissolve` and wider ash-grey in
>    `dissolve_ash`.
>
> One note, not a finding: the bottom dissolve sprites (threshold 0) have no holes, but a few
> rim-coloured specks sit on their edge.
>
> The numeric half stays machine-side and green on all three runners: the pipeline count, the draw
> calls, and no compile after the warm-up. Kept as the procedure — a change under the right-hand
> column of the table re-runs it.
>
> **Corroborated the same day on Linux** (Nobara, Intel UHD 620, Vulkan), where the harness runs
> `--selftest` rather than `--golden` for the reason two paragraphs below: `[gpu] Intel(R) UHD
> Graphics 620 (KBL GT2) | Vulkan | BC: yes`, then `warm-up: 3 pipeline(s) for 7 material(s), 0
> fallback(s)`, `frame: 28 instance(s) in 3 draw call(s)`, `painted: 30.4% of the frame`, the
> `fs_nope` fallback line and `material-gpu: PASS`. Every counter is identical to the Metal run,
> `painted` included to the tenth of a percent — the scene the reference describes is the scene a
> foreign driver draws, which is the part of the gate a pinned PNG cannot carry across adapters.

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

<!-- gate: closed 2026-10-03 -->

> **Closed 2026-10-03** on a MacBook with an M3 Pro (Metal), in a window 1144 px tall. The machine
> half is `[game] materials: on (3 pipeline(s), 0 fallback(s))`. The eye half was judged on a
> screenshot of the game window every few frames, measured per pixel:
>
> 1. **The flash is per instance.** An enemy that has just entered at x≈1848 reads rgb 201,137,161
>    (r−b 40). One at x≈57, about to reach the hero, reads 228,143,134 (r−b 94), and the values
>    in between rise monotonically. In one frame, enemies at x 386 and x 1874 carry different tints
>    (226,146,145 against 200,137,161), so there is no lockstep pulse.
> 2. **The red ring follows the boss's triangle, not its box.**
> 3. **The switch happens once.** The ring stays steady at about 3520 red pixels down to a quarter of
>    the boss's health. From then on it reads 0 until the boss dies, and no frame flickers back. As
>    the holes widen, the boss's lilac area falls 3952 → 3506 → 2394 → 829, with a light-grey rim on
>    the hole edges. After the restart, a healthy boss is ringed again.
>
> **Caveat:** the run that answered question 3 had to land the boss hits from an autopilot. It used
> a local patch that stops the player losing lives in `example_ugly_game/combat.cpp`. The patch
> touches only the player, not the boss, the enemies or the materials, and was reverted and rebuilt
> afterwards.
>
> The bundle path stays machine-side: the bundle matches its sources byte for byte on three OSes,
> and the run-splitting numbers are asserted headlessly. Kept as the procedure — a change under the
> right-hand column of the table re-runs it.

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

<!-- gate: closed 2026-10-03 -->

> **Closed 2026-10-03** on two machines. Everything about hot-reload that a machine can assert is
> asserted twice and headlessly: `material_hot_reload` proves the cache-level contract on every
> OS in CI (a valid edit rebuilds all three pipelines; a broken one is refused, counted, and leaves *the same pipeline objects* drawing),
> and `editor_shell --gate3` proves the panel half by pixels — the preview hash changes on a valid
> edit and is **byte-identical** after a broken one. What neither can answer is the only question the
> feature exists for: does the picture on a real screen change while you keep playing, and does a
> typo leave it alone instead of blanking it.
>
> **Run on 2026-10-03 on a MacBook with an M3 Pro (Metal): everything a still frame can show
> passes.** What is left is the transient: a hitch in the game's frame rate or a flicker of the
> panel at the moment of a reload. The captures were taken about four a second, which is too
> coarse to see either, and that one look was taken on Linux
> the same day, in the block after this one.
>
> - **Editor, machine half:** `--gate3` printed `-> 6 pipeline(s) total`, the
>   `rejected: …:1:1:` diagnostic and `gate 3: PASS (failures: 0)`, and exited 0.
> - **Editor, window:** the panel reads `native watch`.
>   1. The real edit turns the `flash*` column magenta and moves the counters to
>      `6 pipeline(s), 1 reload(s)`.
>   2. The broken edit adds `1 rejected` and a red
>      `sprite_effects.wgsl:1:1: error: expected global item…`. The preview is byte-identical to
>      the frame before the edit, once shifted down by that one new line (34 px).
>   3. Restoring the file gives `2 reload(s)` with `rejected` still at 1, and a preview
>      byte-identical to the one before any edit.
> - **Game:** it printed the `(native watch)` line, then `-> 6 pipeline(s) total`, the diagnostic
>   with `rejected, previous library still drawing`, and `-> 9 pipeline(s) total` on the restore.
>   One window capture per ~0.25 s shows a live enemy pink before the edit. It is a magenta quad
>   after the valid edit, and still magenta through the broken one — about 3 s, 3072 magenta px a
>   frame, never blank. After the restore the magenta reads 0 and enemies flash pink again.
>
> **The transient, 2026-10-03 on the Nobara box — Intel UHD 620, Vulkan, GNOME on Wayland with
> the windows as XWayland clients, 59.96 Hz, on AC — which is what the macOS pass left open.**
> Stills are the wrong instrument for it, so the window was read with `XGetImage` over a small
> rectangle at about 1300 captures a second, twenty times the refresh: a one-frame flicker has
> nowhere to hide between two samples.
>
> - **The panel does not flicker.** Over 22 s carrying four saves (valid → broken → valid →
>   restore) the preview rectangle took exactly **five states, three of them unique**, and every
>   transition was a single step: original → magenta on the valid edit → the frame shifted by the
>   red diagnostic line → back to the *byte-identical* magenta hash → back to the *byte-identical*
>   original hash. No blank, no flat colour, no intermediate frame of any kind. On screen the change
>   lands 0.07-0.08 s after the engine prints the reload.
> - **The game's frame rate does not stumble.** The same sampler timestamped every distinct frame of
>   a moving region: median interval 16.68-16.71 ms — the 59.96 Hz refresh — and p99 19.3-23.4 ms.
>   Over three runs and eighteen reload events the frame straddling a reload measured 8.4-24.5 ms;
>   seventeen of the eighteen sit inside the idle jitter and one reached 24.5 ms once. The worst
>   interval of each run (52.8, 55.5, 60.8, 65.4 ms) never falls on a reload, and a control run with
>   no edits at all carries the same tail — max 52.8 ms. Wall clock agrees: 1200 frames take 20.65 /
>   20.79 / 20.88 s with nothing edited and 20.89 s with four saves in flight.
> - **Editor counters, saving atomically:** `3 pipeline(s), 0 reload(s), 0 rejected, 3 draw call(s),
>   native watch` → valid `6, 1, 0` → broken `6, 1, 1`, where neither pipelines nor reloads move →
>   fixed `9, 2, 1`, `rejected` stops growing → restored `12, 3, 1`. The broken frame's preview is
>   byte-identical to the frame before it once shifted down by the one new diagnostic line: 0 of
>   59 800 pixels differ, maximum channel difference 0. The 17 px here against the 34 px on the
>   MacBook is the font, not the panel.
> - **The game, from its own stdout:** `[game] materials: on (3 pipeline(s), 0 fallback(s))` and
>   `[game] shader hot-reload: … (native watch)`, then `-> 6 pipeline(s) total` on the valid edit,
>   the `:1:1:` diagnostic with `rejected, previous library still drawing` on the broken one — and
>   the enemies keep drawing the magenta of the *previous* edit through it: 1824-9216 magenta pixels
>   a frame for the two seconds the broken file is on disk, never blank. The valid edit that follows
>   reloads (`-> 12`, `-> 15 pipeline(s) total`) and the next frame is magenta again; the zero
>   magenta later in that run is the boss wave, where nothing on screen uses `fs_flash` at all.
> - **Saving in place counts twice.** A save that truncates and rewrites fires two inotify events,
>   so one edit moves the counter by two (`-> 6`, then `-> 9`); a save by temp file and rename,
>   which is what editors do, gives exactly one. The watcher does not debounce, and the panel counts
>   events rather than saves — worth knowing before reading a counter as a number of edits.

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

<!-- gate: open | освещённый кадр на живом GPU: куда падает свет и где ложится тень -->

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
>
> **Run on 2026-10-03 on the Metal reference machine (M3 Pro): the numbers are bit-exact and seven
> questions of eight pass. Question 7 does not, so the gate stays open.** The numbers were
> `mean=0.00000 max=0.00000 frac=0.00000`, `light-gpu: PASS`, lights 5/5, normals 5/2/0,
> occluders 4/3/0, cost 1.462 / 2.636 ms. Questions 1–6 and 8 come back yes.
>
> In question 7, the orange streaks from `key` are there: they point right and down, solid behind
> the disc and striped behind the grate. The blue ones from `fill` are not. `fill` at `0.44, 0.16`
> sits INSIDE the row-2 `outline_danger` disc, and the occluder under the light blocks every ray it
> casts:
>
> - A fill-only render reads `0 1 1` at (365,151) beside it.
> - The same render with `fill` moved to `0.44, 0.50` lights that row: `5 10 26` at (365,90).
>
> A smaller oddity: the solid disc leaks 1–3 LSB radially. The fix is the owner's choice. Either
> move `fill` out of the disc in `lights.txt` and re-bake the reference on this machine, or have
> [`shaders_light.cpp`](../engine/render/shaders_light.cpp) stop a light being blocked by the
> occluder it sits in.

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

<!-- gate: closed 2026-10-04 -->

> **Closed 2026-10-04** by the Windows half at the foot of this section, on the same Intel UHD 620
> box as the Linux half of 2026-10-03 — the slowest machine in the set, and this gate decides on both
> OSes. Eight runs each side, every one exiting `0` with no `FAIL` line and both peers printing
> `over 417 ticks`, `forced=0` and `aliens=0`. The worst network frame of the sixteen is **1.802 ms,
> 10.8% of a 16.67 ms frame** (Windows, `send`), against 0.542 ms and 3.3% on Linux; added to the
> physics step and character tick this box pays anyway, worst on worst on worst that is 6.53 ms —
> **39% of a frame**. Two findings, both about *which* peer pays: question 4's asymmetry comes out
> inverted on Windows in all eight runs — `send` is the costlier peer and it is the one whose socket
> spins — and the receiver's rollback counters are not the same on the two OSes (20/72 on Linux,
> 13/32 here), so they are not pinned by the scripted route the way the sender's 21/84 are.

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
   which is why the script sums them per peer rather than quoting four numbers. Since the #21 audit
   the script *compares* that sum with the budget itself and exits non-zero on
   `FAIL: кадр пира <role> не влезает в бюджет 16.67 мс` — before it, the comparison was announced
   in a comment and never made, so a frame at 200% of the budget printed its percentage and reached
   this report green. What is still yours to judge is what the script cannot see: add the physics
   `heap: mean=` and the character `target: mean=` from §4 and §5 on top, because a real game frame
   carries all of them and the budget is shared.
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

### The Linux half, 2026-10-03 (Nobara, Intel UHD 620, gcc 16.2.1, Release, idle box on AC)

This is the slowest machine in the set, and the one `owner-setup.txt` pins as such. Eight runs of
`scripts/owner_net_budget.sh` on a quiet desktop, every one exiting `0` with no `FAIL` line:

| | worst frame | of 16.67 ms | `sim worst` | `net worst` | socket passes | `resent` |
|---|---|---|---|---|---|---|
| `send` | 0.269-0.456 ms | 1.6-2.7% | 0.234-0.361 ms | 0.027-0.095 ms | 452-558 | 78-224 |
| `recv` | 0.250-0.542 ms | 1.5-3.3% | 0.207-0.338 ms | 0.027-0.204 ms | 1158-1273 | 0 |

Both peers printed `over 417 ticks`, `forced=0` and `aliens=0` in every run, so questions 1 and 2
are answered by the run itself. The counters were identical run to run — `send` 21 rollbacks and 84
replayed ticks, `recv` 20 and 72. That reads like the scripted route being the same route rather than
the machine, and for the sender it is: Windows prints the same 21/84. For the receiver it is **not**,
and the Windows half below is where that shows — 13 and 32 there, just as stable across its own eight
runs.

**The shared budget, which the script cannot see.** This box's physics step (§4) is
`heap: worst=4.177 ms mean=3.276 ms` and its character tick (§5) `target: worst=0.2357 ms
mean=0.0278 ms`. Adding the means to the worst network frame of the eight runs gives
**3.28 + 0.03 + 0.54 = 3.85 ms, 23% of a frame**; worst on worst on worst it is
**4.18 + 0.24 + 0.54 = 4.96 ms, 30%**. The network half is the smallest of the three terms by an
order of magnitude: on this machine the frame is paid for by physics, and the rollback, the
recording and the socket together cost about half a percent of it per peer on average
(`sim mean` 0.032-0.052 ms, `net mean` 0.003-0.011 ms).

**Question 4 is answered the other way round here, and that is a finding.** The gate expects `recv`
to be the expensive peer because it is the one that rolls back. Its socket half behaves exactly as
predicted — 1158-1273 passes against 452-558, a ratio of about 2.3, in every single run. Its
simulation half does not: `recv sim worst` was *lower* than `send sim worst` in six runs of eight
and equal in the other two (0.234 against 0.234), where the M3 Pro reference has `recv` at 1.9x
`send` (0.208 against 0.112). The totals follow: `send` came out the costlier peer in four of the
eight runs, and the two sets overlap completely, so on this box the ordering is noise rather than
structure. What is *not* noise is that the asymmetry the gate rests on is visible on Metal and not
here. The counters rule out a different route, which leaves the clock: every `sim` sample on this
machine is two to three times the M3 Pro's, and whatever `recv`'s replay burst adds is inside that
spread instead of above it. **Which of the two it is — the replay burst drowning in a slower
baseline, or the sender paying something the Mac does not — is not judged from this run.**

`resent=` sits at 78-224 on the sender and 0 on the receiver, exactly the reliable layer doing its
job on a loopback. It is not the story the gate warns about: the sender's `net worst` is the
*smallest* number in the whole report (0.027-0.095 ms), so those resends cost nothing measurable.

The machine was quiet, and there is a cross-check for that rather than a claim: `target: worst=0.2357
mean=0.0278` on this run against `worst=0.2600 mean=0.0242` measured on the same box on 2026-09-01 —
the same numbers, where a loaded box turns them into 3.7 ms (§5 recorded a fourteenfold difference
from three builds running alongside).

**The Windows half runs on this same box**, and it is next: the box is the slowest machine on both
OSes, and §5 closed only after both.

### The Windows half, 2026-10-04 (Windows 11 build 26200, MSVC 14.44.35207, Release, idle box on AC)

Same box, and `8a163ab` on it: `cmake --build build --target game_platformer_net_test` answered
`ninja: no work to do`, which it can because that commit touches docs, gate scripts and two Tiled
files and no engine source. Eight runs of `scripts/owner_net_budget.sh` from git-bash on a quiet
desktop, every one exiting `0` with no `FAIL` line, both peers printing `over 417 ticks`, `forced=0`
and `aliens=0` in all eight — so questions 1 and 2 are answered by the runs themselves.

| | worst frame | of 16.67 ms | `sim worst` | `net worst` | socket passes | `resent` |
|---|---|---|---|---|---|---|
| `send` | 0.819-1.802 ms | 4.9-10.8% | 0.615-0.915 ms | 0.204-0.887 ms | 571-572 | 256-257 |
| `recv` | 0.684-1.275 ms | 4.1-7.6% | 0.555-0.715 ms | 0.129-0.644 ms | 257-271 | 0 |

The means over the same eight: `sim mean` 0.078-0.099 ms on `send` and 0.072-0.095 on `recv`,
`net mean` 0.036-0.047 and 0.031-0.039. The Linux half's were 0.032-0.052 and 0.003-0.011 — about
twice on the simulation, about five times on the socket, and the socket is where the gap is widest.

**The shared budget, which the script cannot see.** This box under Windows steps physics (§4) at
`heap: worst=4.398 ms mean=3.589 ms` and ticks the character (§5) at `target: worst=0.3339 ms
mean=0.0302 ms`, measured immediately after the eight runs on the same quiet desktop. Means plus the
worst network frame of the eight gives **3.59 + 0.03 + 1.80 = 5.42 ms, 33% of a frame**; worst on
worst on worst, **4.40 + 0.33 + 1.80 = 6.53 ms, 39%**. The Linux half paid 23% and 30% for the same
three terms, and nearly all of the difference is the network one: 1.80 ms here against 0.54 there.
**On Windows the network frame stops being a rounding error.** On Linux it was an order of magnitude
below physics; here it is the second-largest of the three terms and within a factor of two and a half
of the solver.

The box being quiet is cross-checked rather than claimed, the same way the Linux half does it: §4
recorded `heap` mean **3.560 ms** on this box under Windows on 2026-08-22 against 3.589 now, 0.8%
apart, and §5 recorded `target` mean **0.0304-0.0310 ms** over five idle runs on 2026-09-01 against
0.0302 now. Both numbers become multiples of themselves on a loaded box, so agreeing with runs six
weeks old is the evidence that nothing was building alongside.

**Question 4 is inverted here, and harder than on Linux.** The gate expects `recv` to be the
expensive peer because it is the one that rolls back, and expects its socket to make roughly twice
the passes because it spins waiting for input the sender never waits for. On this box both halves of
that come out backwards, in every run:

1. `send` is the costlier peer in **eight of eight**. On Linux it was four of eight with the two sets
   overlapping completely, which is noise; this is not noise — `send` 0.819-1.802 against `recv`
   0.684-1.275, and the per-run ordering never once flips.
2. The socket passes are `send` 571-572 against `recv` 257-271, a ratio of **2.2 in favour of the
   sender**, where Linux has 1158-1273 against 452-558 — 2.3 the other way. The receiver here barely
   spins at all.
3. `net worst` is the larger one on `send` in eight of eight, and it is what carries the totals: the
   two `sim worst` spreads overlap completely (`send` 0.615-0.915, `recv` 0.555-0.715, with `recv`
   the higher one in four runs of the eight), so the inversion is the socket, not the simulation.

**And the counters say why.** `recv` prints `rollbacks=13 replayed=32` in every one of the eight runs
here, against `rollbacks=20 replayed=72` in every one of the eight on Linux, while `send` prints the
same `21`/`84` on both. The receiver's rollback depth is therefore **not** pinned by the scripted
route — it is pinned by when the late input lands, which is the socket's business, and the inference
the Linux half drew from identical counters holds for the sender only. The two findings are one
story: on this box the loopback delivers input early enough relative to the tick that the receiver
spins about a third as much and rolls back about a third as far, which leaves the sender — with its
256-257 resends, each one a pass — as the peer paying for the wire. `resent=` on the receiver is `0`
here as it is on Linux, and question 5's warning does not fire: those resends sit beside a `net
worst` that stays under a third of a millisecond in four runs of the eight.

**What these runs do not say** is whether Winsock genuinely delivers earlier than the Linux loopback
or the two peers merely interleave differently under a different scheduler. The replay counts are the
symptom, and nothing in this output separates the causes. What they do answer is the gate's own
question: 39% of a frame worst on worst on worst, with the sender as the expensive peer, is a frame
that fits.

## 14. Gate 9 of #22 — a live session (two machines)

<!-- gate: open | ДВЕ машины, один коммит: прогон A сходимость по проводу (--listen/--at), прогон B живая сессия с окном на обеих сторонах (game_platformer_net_live, 3600 тиков, Esc = код 10) -->

> **Both halves of this gate became runnable on 2026-09-06.** The convergence half — two processes
> on two machines, one input, one state, over a wire with real latency — has been runnable since
> 2026-09-04, when the peers stopped hard-coding `pnet::ADDRESS_LOOPBACK` and started taking the
> neighbour as an argument. The other half, *does it feel like one game*, could not be asked at all:
> the peer was headless by design and there was nothing to look at. It now has a window on **both**
> sides — `game_platformer_net_live`, a second target over the **same** `run_peer` loop, reached
> through a seam (`PeerHooks`) rather than a second copy of the rollback cycle. A copy would have
> drifted from the original in silence: the gates would keep agreeing on theirs while you played
> another. So there are two runs below, and step B is the one nobody but you can do.

### What you need

Two machines on one network — any pair of Linux, Windows and macOS, mixed is better than matched —
**both built from the same commit**. Not a formality: the whole claim is that identical inputs give
identical state, and two different commits would answer a question nobody asked. Check with
`git rev-parse HEAD` on both.

Run **B** needs a screen, a GPU and a keyboard on each of them; run **A** needs neither and is the
one to start with, because a pair that cannot converge headless will not converge with a window in
front of it either.

Note each machine's address (`ip addr` on Linux, `ipconfig` on Windows, `ipconfig getifaddr en0` on
macOS) and let UDP through the firewall on **both** ports below — each machine listens on one and
sends to the other, so `7777` and `7778` are each open on one side. Windows will ask on the first
run, Linux may need `sudo ufw allow 7777/udp` on A and `sudo ufw allow 7778/udp` on B. Measure the
wire first, because the answer at the end is read against it:

```
ping -c 20 <the other machine>
```

### Run A — convergence, no window

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

### Expected output of run A

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

### What to judge in run A, in order

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
   | `10` | somebody pressed Esc or closed the window — run B only, and it is an answer, not a failure; `10` together with `[platformer] surface texture status <n> - quitting` or `[gpu] device lost (reason <n>): …` means the window closed itself — that is a finding, report both lines |
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

### Run B — a live session, a window on each side

Build the live peer on both machines:

```
cmake --build build --target game_platformer_net_live
```

The arguments are the ones you already typed in run A — same parser, same strictness, because these
are typed by hand on two machines and a silently accepted `--lisen 7777` would send a peer off to
meet a neighbour through a file that does not exist on the other box:

```
./build/game_platformer_net_live --peer send example_ugly_game/assets/game.bundle live \
  --listen 7777 --at <B>:7778
```

```
./build/game_platformer_net_live --peer recv example_ugly_game/assets/game.bundle live \
  --listen 7778 --at <A>:7777
```

Machine **A** plays; **B** watches the same hero move under the same physics. Both windows open
before the rendezvous — the neighbour is waiting on a deadline, and a side that spent those seconds
creating a wgpu device would eat them out of somebody else's eight.

**The session is exactly 3600 ticks — one minute at 60 Hz — and both sides know that number in
advance.** It is not negotiated over the wire: the peer waits for acknowledgements up to `total`,
so a side that decided to play longer would run into its neighbour's deadline, and the end of the
session would look like a dropped connection. The tick is paced by the **wall clock**, not by the
screen: `Fifo` waits for vsync, so a minute measured in frames would be thirty seconds on a 120 Hz
monitor and two on a 30 Hz one.

**Esc, or closing the window, ends the run with code `10`** and a line saying so. That is a refusal,
not a crash — but it is one-sided: the neighbour keeps waiting and leaves on its own deadline
(`4`) two minutes in. That deadline is twice the session and is derived from it, not written down
as a number: written down, it once came out equal to a 30 Hz session, so an intact pair would have
been read as "the neighbour walked away". Quitting early means both sides quit early.

Each side prints one line before the session and the same three afterwards. The two sides do not
print the same line — only the sender opens a gamepad, because input travels one way:

```
peer send: 3600 ticks, gamepad backend <name>, Esc quits
peer recv: 3600 ticks, watching the other side, Esc quits
```

**One more line may appear mid-session**, and it is not an error:

```
live: <n> ticks behind the wall clock, that debt is dropped (<total> total)
```

The machine slept, or the window was dragged, and wall-clock time passed without ticks being
played. Anything past a dozen ticks of debt is dropped rather than played out at once — a dozen
ticks played without a single keyboard poll is a hero moving by itself. Seeing this line once when
you dragged the window is expected; seeing it while you play, on a machine doing nothing else, is a
finding: write down what the number was and what was on screen.

**The second number is the running total, and it is what tells code `4` apart from a dead
neighbour.** Dropping debt gives back the *pace*, not the ticks: the session still owes 3600 of
them, while the deadline is measured on the wall clock. A machine that has dropped, in total, more
than a session's worth of ticks will leave on code `4` with an intact network and a neighbour that
never went anywhere — and step 5 below tells you to read that as "the neighbour walked away". So
when either side ends on `4`, read the totals first: a large one means the finding is about this
machine, not about the wire.

### What to judge in run B, in order

1. **Both windows opened and both showed the same hero in the same place.** Exit code `1` means the
   window or the wgpu device did not come up, and the reason is on stderr; that is a local problem,
   not a network one, and worth reporting with the GPU and driver.
2. **Whether it plays.** This is the whole reason the run exists and no gate can ask it: press left
   on A and watch B. Does the input feel attached to the hero, or does it arrive late? Does the
   picture on B stutter, snap back, or slide? Rubber-banding on B is a rollback you can see — write
   down roughly how often and after what (a jump, a direction change, standing still).
3. **The same two constants, now with a picture.** `PEER_PREDICT = 4` and `PEER_DEPTH = 8`
   ([`platformer_peer.hpp`](../example_ugly_game/platformer_peer.hpp)): four ticks of prediction is
   66 ms, chosen against a loopback whose latency is zero. They are the network analogue of the 500
   bodies spec #15 claimed and your run cut to 350 — numbers picked where the measurement was cheap,
   waiting for the machine that decides. Judge them against the `ping` figure you took at the start:
   if the play feels attached at 20 ms and detached at 80 ms, that is the finding, and the numbers
   from step 4 of run A say which of the two constants to move.
4. **`ticks=3600` and `forced=` on both sides at the end.** Same reading as run A, on a run whose
   input came from a person instead of a script — which is the one thing the automated gate cannot
   produce. `forced > 0` here with `forced == 0` in run A over the same wire is worth reporting on
   its own: it would mean human input has a shape the script does not.
5. **If a side ended on code `4`, read the `live:` totals before blaming the wire.** Code `4` is
   "the neighbour walked away", and that is the right reading when the other side is gone — Esc,
   a closed window, a killed process. It is the wrong reading when this machine spent the session
   dropping debt: the deadline is wall-clock, the dropped ticks are not, and enough of them ends
   an intact pair on `4`. No `live:` line and a `4` is a network finding; a large total and a `4`
   is a finding about the machine, and the total is the number to write down.

### What the automated half already says about this seam

The seam itself — that the live half feeds input and shows frames through `PeerHooks` without
changing the run — is not left to this page. `game_platformer_net_hooks_test` runs the same pair
twice, once through the seam with a script behind it and once without, and requires the same mark
and the same recording byte for byte. Nine stand-in live halves prove the assertions can fail and
that every branch of the seam is really asked: a sender that never moves, one that answers "not yet"
every other question, one that names half a script as the session, one that names a one-millisecond
deadline, a display that refuses on its first frame, a mode nobody implemented (which must be
refused with its own code `11`, not run without the seam), an input with the depth axis `move_z`
the sample's wire does not carry (the sender leaves with its own code `12` instead of stalling to
the deadline), and a pair whose sender is refused one
send — the sample it already took from the seam has to survive that refusal, because a person
cannot be asked twice about the same tick, and the same pair with that cache broken has to play a
different run. What none of them can do is look at a screen, which is why run B is here.

## 15. Gates 1 and 3 of #20 — the engine package runs where it was not built

<!-- gate: open | пакет движка запускается там, где не собирался: .dmg, .AppImage и .msi двойным щелчком на машине без этого дерева -->

> **All three packages are runnable from one machine today.** Verticals 1–3 of spec #20 build the
> **macOS** package on the host, the **Linux** package in a container on that same host, and the
> **Windows** package in a CI job whose artifact the script downloads and verifies. What needs your
> machines is the other half of the claim: a package built here has to run **there**, on a box that
> never saw this source tree. Vertical 4 adds the form people actually install from — the `.dmg`,
> the `.AppImage` and the `.msi` — and each of those has its own subsection below, because a gate
> can assert what is inside a package but not what happens when someone double-clicks it.

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
like-nes-engine-v0.1.0-macos-arm64.dmg          <bytes>     12  <64 hex>

внутри: like-nes/bin/assetc like-nes/bin/editor_shell like-nes/bin/libwgpu_native.dylib …
манифест: release/v0.1.0/like-nes-engine-v0.1.0-macos-arm64.manifest
суммы:    release/v0.1.0/SHA256SUMS
манифест образа: release/v0.1.0/like-nes-engine-v0.1.0-macos-arm64.dmg.manifest
сумма И РАЗМЕР .dmg от прогона к прогону разные (hdiutil пишет в UDIF время и UUID);
воспроизводится СОДЕРЖИМОЕ образа — его сверяет scripts/check_release_dmg.sh
```

The `.dmg` row is the one line in that table that **changes between runs**, and that is not a
finding: UDIF carries a build timestamp and a UUID, so the image's bytes cannot repeat. What repeats
is the image's **contents** — names, modes, per-file sums — and that is what the gate compares. Do
not eyeball the two `.dmg` sums. The image holds one file more than the archive: the bundle's
`Info.plist`.

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

### The `.dmg` — how people actually install it

The tarball is delivery for someone who already lives in a terminal. A normal macOS install looks
different, and that path needs your hands too:

```
open release/v0.1.0/like-nes-engine-v0.1.0-macos-arm64.dmg
```

6. A volume window opens holding `like-nes.app` next to an `Applications` shortcut.
7. Drag `like-nes.app` onto `Applications`, eject the volume, then launch
   `/Applications/like-nes.app` by double-clicking it in Finder.
8. The editor window opens. A `Library not loaded` error here means what it meant in point 2, with
   an addition: the dylib has to sit **next to** the executable, in `Contents/MacOS`, because our
   rpath is `@executable_path` — a bundle that hides it in `Contents/Frameworks` looks assembled and
   starts nothing.
9. Gatekeeper will say it "cannot verify the developer" on an unsigned bundle. That is **expected**
   — there is no signing or notarisation in this round. Open it through the context menu → Open.
   It is its own point because without it step 7 reads as the package refusing to run.

Points 6–9 are the part no runner can assert: there is nothing there to click with. Everything else
about the image is asserted by `bash scripts/check_release_dmg.sh` — the bundle's contents by name,
its contents against the installed stage, modes (executable under `Contents/MacOS`, not under
`Contents/Resources`), the `/Applications` symlink, `Info.plist` (entry point, version, commit),
licenses, the stamp, how the bundle finds the runtime (`@rpath/libwgpu_native.dylib` in the
executable plus a relative `LC_RPATH` — the two halves of point 8, read with `otool`), and that
**two runs produce the same contents**. On Linux and Windows it skips
out loud: `hdiutil` exists only on macOS.

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
like-nes-engine-v0.1.0-linux-aarch64.AppImage   <bytes>     15  <64 hex>

внутри: like-nes/bin/assetc like-nes/bin/editor_shell like-nes/bin/libwgpu_native.so ...
манифест образа: release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.AppImage.manifest
сумма .AppImage ПОВТОРЯЕТСЯ между прогонами (squashfs зависит только от содержимого и
mtime, а mtime выравнен штампом коммита) — её сверяет scripts/check_release_appimage.sh
release: пакет Linux собран: release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.tar.gz
release: образ AppImage собран: release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.AppImage
```

Note how this reads **against** the macOS table above: there the `.dmg` row is the one line that
changes between runs and you are told not to compare it; here the `.AppImage` row is expected to
repeat byte for byte, and two runs disagreeing **is** the finding. The difference is the format, not
our diligence — UDIF carries a timestamp and a UUID, squashfs carries neither. The image holds four
files more than the archive: `AppRun`, the `.desktop` entry, the icon and its `.DirIcon` copy.

The container runs **the same `release.sh`** — it supplies a Linux host, not a second packer — so
the table and the contents match the macOS run except for the runtime name (`libwgpu_native.so`).
The tree is mounted read-only and the build directory lives outside it, so `git status` is clean
afterwards. The base image is pinned by **digest**, not by the `ubuntu:24.04` tag: a tag moves to a
new image silently, and a package built by "the same command" would stop being the same package.

Then copy `release/v0.1.0/like-nes-engine-v0.1.0-linux-aarch64.tar.gz` to a Linux box with no source
tree and no toolchain, and run the same four steps as above. Point 3 is where the clean distro's
X11/Wayland dependencies surface — that is gate 5 of this spec, and no runner can answer it.

### The `.AppImage` — how people actually run it on Linux

The tarball is again delivery for a terminal; the one-file form is what gets handed to everyone
else, and it needs your hands on a real distro:

```
chmod +x like-nes-engine-v0.1.0-linux-aarch64.AppImage
./like-nes-engine-v0.1.0-linux-aarch64.AppImage
```

6. The editor window opens with no unpacking, no install and no source tree. A `libwgpu_native.so:
   cannot open shared object file` error is the Linux twin of point 8 above: the runtime has to sit
   **next to** the executable in `usr/bin`, because our rpath is `$ORIGIN` — the same mechanism
   `@executable_path` gives us on macOS.
7. On a system with no FUSE (`dlopen(): error loading libfuse.so.2`), `./…AppImage
   --appimage-extract-and-run` is the documented fallback — that is the AppImage runtime speaking
   about the box, not our package failing. The same variable, `APPIMAGE_EXTRACT_AND_RUN=1`, is what
   the container uses, since FUSE there would need `--privileged`.
8. The desktop integration is deliberately minimal: the image carries a `.desktop` entry and an
   icon, but nothing installs them — no `appimaged`, no writes to `~/.local/share/applications`. A
   launcher entry appearing on its own is not expected in this round.

On a Linux box **with no `appimagetool`** (the host build, not the container one) the `.AppImage`
row is absent and a named reason stands in its place — the run still succeeds and the `.tar.gz` is
complete:

```
release: appimagetool не найден — .AppImage НЕ собран, в каталоге только .tar.gz.
```

Silence there is a finding: a skip nobody announced reads exactly like "the image was built".

Points 6–8 are the part no runner can assert. Everything else about the image is asserted by
`bash scripts/check_release_appimage.sh` — composition by name, contents against the installed
stage, modes, licences, the stamp with its triple, `AppRun`, the `.desktop` fields, how the image
finds its runtime (`DT_NEEDED` plus a `$ORIGIN` search path, read with `readelf`), and that **two
runs produce the same bytes**. On macOS and Windows it skips out loud, and so it does on a Linux box
with no `appimagetool`: the tool is pinned by sha256 inside the container image, not installed into
your system.

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
3. `editor_shell` opens a window. A demand for the Visual C++ Redistributable is **gate 4** of this
   spec and, since vertical 5, a finding rather than an open question — §16 says what the package
   now carries and what asserts it.
4. `like-nes\licenses\` holds seven files, none empty (regression of spec #9).

`bash scripts/check_release_ci.sh` asserts the rules of the path (what the job builds, that it
publishes nothing, how the run is picked) without touching the network — that is the form inside
`scripts/preflight.sh`. `--live` runs the whole orchestrator: dispatch, wait, download, chain.

### The `.msi` — how people actually install it on Windows

The tarball is delivery for someone who already lives in a terminal. A normal Windows install is a
double click on an installer, and the CI run now hands one over beside the archive:

```
release/v0.1.0/like-nes-engine-v0.1.0-windows-x86_64.msi
```

5. Double-click the `.msi` on a Windows box with no source tree and no MSVC. It must install
   **without an administrator prompt**: the install root is `LocalAppDataFolder` and `ALLUSERS` is
   not set, so it is a per-user install. A UAC prompt here is a finding, not a formality.
6. SmartScreen will say the publisher is unknown on an unsigned package. That is **expected** —
   there is no code signing in this round, for the same reason there is no notarisation on macOS.
   Choose "More info" → "Run anyway". It is its own point because without it step 5 reads as the
   installer refusing to work.
7. A Start-menu shortcut appears and opens the editor. The shortcut points at `editor_shell.exe`,
   not at the baker.
8. Installing the same version again, and then a **newer** version on top, must leave **one**
   entry in "Installed apps", not two: the `UpgradeCode` is constant across the product line and the
   `ProductCode` is derived from the version, so the installer recognises its own earlier build and
   removes it. Then try the other direction — an **older** package on top of a newer install. It
   must refuse with "A newer version of like-nes engine is already installed", not install and
   silently replace the newer one. The direction matters: the removing range in the `Upgrade` table
   ends at this version, and a range left open above it makes the downgrade look like a normal
   upgrade.
9. Uninstall from "Installed apps", then look at
   `%LOCALAPPDATA%\like-nes` — the directory must be **gone**, not left behind empty. This is gate 7
   of spec #20, and it is the whole reason the format is MSI rather than a hand-written script.

Points 5–9 are the part no runner can assert: there is nothing there to click with, and the
Windows runner has no `msitools` to look inside a package with. Everything else about the installer
is asserted by `bash scripts/check_release_msi.sh`, which builds the package **twice** and compares
the two runs' **contents** — every table plus the extracted tree. Its sum changes between runs while
its byte size stays the same, and that is not a finding: an MSI carries a random package code and a
build timestamp written in place,
the same boundary the `.dmg` has and the opposite of the `.AppImage`. On top of the contents it
asserts what the extracted tree cannot show — the per-user install of point 5, the shortcut target
of point 7, the upgrade line of point 8 — both of its ranges, the one that removes and the one that only
detects — and the completeness of the uninstall of point 9
(`RemoveFolder` on every directory we create, plus every file living in a component that is actually
part of the installed feature). The installer that arrives from CI is inspected the same way by
`scripts/check_release_ci.sh`. Where the MSI compiler or `msitools` is missing, both skip **out
loud** with code 0: `brew install msitools` on the owner's machine.

## 16. Gate 4 of #20 — no Visual C++ Redistributable, and a silent install

<!-- gate: open | Windows без VC++ Redistributable: тихая установка .msi и editor_shell --gate6 на чистой машине -->

> **This is the half of gate 4 that a gate can prepare but not close.** Vertical 5 made the Windows
> package self-contained: our binaries are built with the **static** CRT
> (`cmake/msvc_runtime.cmake`), and the one dependency we do not compile — the prebuilt
> `wgpu_native.dll`, built by a foreign toolchain against the dynamic CRT — gets its
> `VCRUNTIME140.dll` shipped **beside it** in the package (`cmake/msvc_redist.cmake`). The UCRT
> (`api-ms-win-crt-*`, `ucrtbase.dll`) is deliberately *not* shipped: it is an OS component from
> Windows 10 on, and carrying it would be substituting for the operating system. Windows 8.1 and
> older are out of scope, and that is a statement, not an oversight.

The machine half runs anywhere, with no Windows and no package:

```
bash scripts/check_release_crt.sh
```

Expected on a macOS box with no Windows package in `release/` — five lines, and the fifth is a
skip said **out loud**, because a silent zero would read exactly like a passed gate:

```
crt-check: OK   статический CRT задаётся до зависимостей (строка 47 против 67)
crt-check: OK   копии закрытого списка redist-имён совпадают (8 префиксов)
crt-check: OK   якорь читателя: настоящий wgpu_native.dll разобран (13 импортов)
crt-check: OK   app-local DLL в ожидаемом составе совпадают с импортами рантайма (1)
crt-check: ПРОПУСК — пакета Windows в release/ нет (собирается в CI), осматривать нечего
crt-check: PASS
```

With a Windows package present (after `bash scripts/release.sh --only windows --version v0.1.0`)
the skip line is **replaced**, not appended to — the gate names the package it chose and then makes
the two assertions the skip stood for, so seven lines come back instead of five:

```
crt-check: OK   статический CRT задаётся до зависимостей (строка 47 против 67)
crt-check: OK   копии закрытого списка redist-имён совпадают (8 префиксов)
crt-check: OK   якорь читателя: настоящий wgpu_native.dll разобран (13 импортов)
crt-check: OK   app-local DLL в ожидаемом составе совпадают с импортами рантайма (1)
crt-check: осматривается like-nes-engine-v0.1.0-windows-x86_64.tar.gz
crt-check: OK   ни одной недостающей DLL из VC++ Redistributable (4 бинарей осмотрено, 2 исполняемых)
crt-check: OK   наши бинари не просят VC++ Redistributable (2 осмотрено, чужие исключены поимённо)
crt-check: PASS
```

The counts in the last two lines depend on the package, so read them as shape, not as a golden: what
matters is that neither is zero. Those are two assertions, not one — while `vcruntime140.dll` is in
the package, anything may point at it, so losing `CMAKE_MSVC_RUNTIME_LIBRARY` would be a silent fall
back to a file that is already there. The first one looks for each DLL in the directory of the
**executable**, not next to whoever imports it: app-local search on Windows goes by the process
directory, and today both are the same `bin/` — after the first file moves, "somewhere in the
package" would pass a package that does not start.

The fourth line runs with no package at all, and its subject is the one name in this vertical still
written by hand: `like-nes/bin/vcruntime140.dll` in `expected_files` (`scripts/release_check_lib.sh`).
Nothing mechanical tied it to what the runtime actually imports, so it is tied here — the expected
composition is compared, case-folded, against the import table of the real `wgpu_native.dll`. Zero
redist imports on that side is a **failure**, not "the lists agree": empty equals empty, and the
subject of the app-local measure would have quietly disappeared (the `mirrors-group` shape from
`ci_lint.py`). No distribution checked out, no comparison — and the gate says so out loud.

### The box half — a Windows that never had a compiler

The point of this section is a machine on which the redistributable was never installed, so check
that first; the answer is part of the result:

```
where vcruntime140.dll
```

Expected: `INFO: Could not find files for the given pattern(s).` A path under `C:\Windows\System32`
means some other product installed the redistributable, and this box cannot answer gate 4 —
the package would start there whether or not it carried its own copy. Say so rather than sending a
green line from a dirty box.

Then, from the unpacked archive of §15:

```
dir like-nes\bin
like-nes\bin\assetc.exe --synthetic %TEMP%\probe.bundle
like-nes\bin\editor_shell.exe
like-nes\bin\editor_shell.exe --gate6 %TEMP%\crt-boundary.png
```

1. `like-nes\bin` holds **four** files: `assetc.exe`, `editor_shell.exe`, `wgpu_native.dll` and
   `vcruntime140.dll`. The last one is the package carrying its own dependency; its absence is the
   finding, not its presence.
2. Both binaries start. A dialog saying *"The code execution cannot proceed because VCRUNTIME140.dll
   was not found"*, or any prompt to install the *Microsoft Visual C++ 2015–2022 Redistributable*,
   is a finding — and worth a screenshot, because it means the copy step or the static CRT was lost
   somewhere between here and the runner.
3. `--gate6` writes `%TEMP%\crt-boundary.png` and the process exits `0`. This is the one step that
   actually walks the **mixed-CRT boundary** vertical 5 creates: our binaries went to `/MT`, the
   prebuilt `wgpu_native.dll` stays on `/MD`, so two heaps live in one process. That is safe only
   because no *ownership* crosses — every wgpu object is released by its own `wgpu*Release`, the
   frame is read between `wgpuBufferGetConstMappedRange` and `wgpuBufferUnmap`, and the PNG is
   written by the stb compiled into **our** build (`engine/render/capture.cpp`). A crash inside a
   free, a hang on unmap, or a zero-byte PNG is the finding this step exists for: no gate on any
   machine can see it, because both halves are only ever in one process on Windows (ADR 0019,
   decision 41).

### The silent install

`/qn` is not a convenience here, it is the assertion. Under it there is nobody to answer a UAC
prompt, so a package that asks for elevation does not install at all — which is exactly what a
corporate rollout would hit:

```
msiexec /i like-nes-engine-v0.1.0-windows-x86_64.msi /qn /l*v %TEMP%\like-nes-msi.log
echo %ERRORLEVEL%
dir %LOCALAPPDATA%\like-nes\bin
msiexec /x like-nes-engine-v0.1.0-windows-x86_64.msi /qn
dir %LOCALAPPDATA%\like-nes
```

4. The install prints nothing and shows **no window at all** — no progress bar, no UAC prompt —
   and `%ERRORLEVEL%` is `0`.
5. `%LOCALAPPDATA%\like-nes\bin` holds the same four files.
6. The uninstall is equally silent, and the last `dir` answers `File Not Found`: the directory is
   gone, not left behind empty. That is gate 7 of the spec again, this time without a single click.
7. `1603` from step 4 with `Error 1925. You do not have sufficient privileges` in the log is the
   failure this section exists for: `msiexec` decides whether to ask for elevation by **bit 8 of the
   summary stream**, which the *compiler* writes from `InstallScope="perUser"` in
   `packaging/like-nes.wxs.in` — not by the install root. A package rooted in `LocalAppDataFolder`
   without that attribute installs into the same directory and still raises UAC.

That bit is asserted on both sides — on the fixture built here (`bash scripts/check_release_msi.sh`,
line `тихая установка: прав не требует (Source=10)`) and on the installer that **arrives from CI**
(`bash scripts/check_release_ci.sh --live`). Two assertions rather than one, because the bit is
written by the compiler and there are two compilers: `wixl` here, WiX v3 on the runner. Their
*input* is the same file; their *behaviour* on this bit is not proven by anything, and the way that
divergence would surface is a UAC prompt under `/qn` on your box, not a red gate.

## 17. Gate 5 of #19 — the install page read by someone who did not write it

<!-- gate: open | getting-started дословно на macOS и Windows: окно, адаптер, играющая игра — того контейнер не покажет -->

> **The machine half of round #19 now runs the page, but it cannot judge it.**
> `scripts/check_docs_start.sh` takes the commands out of the marked blocks of
> `docs/en/getting-started/` — `<!-- container: install|build|run|check -->` — asserts that the two
> languages carry them byte for byte, that the clone address is this repository, that every block
> promising output says so in the prose the reader actually sees, and then (`--live`) replays them
> on a **bare Ubuntu pinned by digest**, with the clone swapped for a copy of this tree. What
> survives that is a page whose commands install, configure, build and print on a machine that has
> never seen this engine. What does not survive it is everything a container has no way to show: a
> window, an adapter, a game that plays, an edit that reloads — and the two operating systems the
> image is not. `scripts/check_docs.sh` still holds the other half of the pair: mirrored trees,
> sha256 stamps, links, anchors, licence texts in both READMEs. None of it says the page is
> *followable*: a page can build clean in a container and still send a stranger to a package it
> never named on the OS you are sitting at.

The page under test is [`docs/en/getting-started/`](en/getting-started/) — `prerequisites.md` (what
the OS has to have), `build.md` (clone and build) and `first-run.md` (the editor, the two sample
games, hot-reload, and what a failure looks like). The Russian pair is
[`docs/ru/getting-started/`](ru/getting-started/).

Run the machine half first, from this machine — it needs a container engine, which no runner here
has:

```sh
bash scripts/check_docs_start.sh --live
```

It ends with `check-docs-start: PASS чистая машина прошла getting-started`. A failure there is a
defect in the page, and finding it costs a quarter of an hour instead of a clean box — so do not
start the manual pass until it is green.

**A clean box is one where this engine has never been built** — not a wiped one, and not the
container above. That container has already answered for Linux, and only for the half of the page
that has no window in it: the editor, the GPU adapter and the hot-reload loop are exactly what it
cannot show, and macOS and Windows are not it at all. Nobara and the Windows box both qualify only
until the first run — so if you are going to do this, do it before anything else on that machine.

1. Read the page **as written**, top to bottom, and type only what it says. Do not fill in a missing
   package from memory, do not add a CMake flag it does not mention, do not use `win-dev.bat` if the
   page did not tell you to. The whole value of the gate is in what you have to add for yourself.
2. Note every place where you had to. Each one is a defect in the documentation, not in your
   patience — the page promises a stranger can follow it.
3. Stop where the page stops: `editor_shell` open on a window, `game_platformer` playing, and the
   hot-reload edit from `first-run.md` visible.

Expected: nothing but the commands on the page, in order, and the last one leaves a window open.
The failures worth writing down are the quiet ones — a package the page forgot (the build stops with
a header not found), a Windows shell that is not the x64 prompt (`cl` is not on `PATH`), a first
`cmake` run that asks about X11 a second time, a path in the text that no longer exists in the tree.

**Gate 6 of the same spec is not runnable yet, and that is a statement.** It asks for the tutorial —
"first game", from an empty project to a playable character — to be reproducible step by step, and
that section is deliberately unwritten: `docs/en/index.md` names `tutorial/`, `guide/`,
`reference/` and the rest **without links**, because a link into nothing is a finding of the link
gate and a lie to the reader either way. When the tutorial is written, this gate is run the same way
as §17 — by typing what the page says and noting everything you had to add.

## 18. Gate 3 of #24 — level 1 comes out of a real Tiled

<!-- gate: closed 2026-10-03/04 -->

> **Closed 2026-10-03/04** on the Windows box — Tiled 1.12.2, MSVC, NVIDIA GeForce MX150 on Vulkan
> — after the Linux run of the day before had settled everything on this gate that a box without
> Tiled can settle. The save is the whole point of the gate, and it did write fields the
> hand-written files never carried: `"opacity":1` on every object of `spawns`, an escaped
> `"..\/assets\/warped-city\/tileset.png"`, and `tiledversion` 1.10.2 → 1.12.2. The importer took
> all three — `bundle_hash` did not move a bit — and the window drew the saved level as Tiled draws
> it. The Windows record is the last block of this section; the Linux one before it is where the
> before-value came from.

`games/neon-rumble/levels/level1.tmj` and `warped-city.tsj` were written by hand to the Tiled 1.10
JSON format, not saved by Tiled. The importer is tested on fixtures of the same hand, so a field
Tiled actually writes and the importer refuses — or one it writes differently — stays invisible
until a real Tiled saves the file. The gate has two halves: the bake (steps 1–4) and the window
(step 5), where the game draws the saved level from `game.bundle` and the owner compares it with
Tiled. Parallax layers and the animated sign came to level 1 with B8a, after this gate closed: three
image layers with `parallaxx`, `repeatx` and a `cover_y` property, and a second hand-written tileset
`neon-signs.tsj` with `animation`. A fresh save of that level is step 2 of §20; flips and
tile animation are judged by the pixel golden on CI on all three OSes.

1. Open `games/neon-rumble/levels/level1.tmj` in Tiled 1.10 or newer. The map is 40×12 tiles of 16
   px: a row of facades over a street, a one-way platform, a ladder, and the object layer `spawns`
   with `player`, `street` and `walk`.
2. Move any tile, undo it, and save the map with **File → Save** (keep JSON, keep CSV layer
   format). Open `warped-city.tsj` in the same window and save it too.
3. Run the gate from the repository root (Windows: from the `scripts\win-dev.bat shell` window):

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS`, and both lines
   `Release: game.bundle bundle_hash 0x4e7f9ade1d27776a matches bundle.hash` and the same for
   `Debug`, each run preceded by
   `neon-rumble: level level1 40x12 tile 16, 2 visual layer(s), 1 texture(s) 384x256`.
4. Send back `git diff --stat games/neon-rumble/levels` and, if the gate failed, its FAIL line.
5. Run the window from the same shell and keep Tiled open next to it:

       ./build-sdk-work/game-Release/neon_rumble --frames 600

   (Windows: `build-sdk-work\game-Release\neon_rumble.exe --frames 600`.) The window shows the
   street and the facades exactly as Tiled draws them, scaled by a whole number (2 on a 1x screen,
   5 on a 2x one); the camera pans along the street and back, then the window closes with
   `neon-rumble: window run ok, 600 frames` and exit code 0. Send back a screenshot and the
   `[gpu]` line. The queen at the left end of the street is the fighter of §19, not part of the
   level; judge the tiles around her.

What counts as a finding: an import refusal (the message names `level1.tmj:<line>:<column>` and
what to change in Tiled), a different `bundle_hash`, or a window that differs from Tiled — a tile
in the wrong place or mirrored, a seam or a line between tiles, a shimmer while the camera pans,
tiles lighter or more washed out than in Tiled (on Linux and Windows the window surface may be sRGB,
and a doubled colour encoding would look exactly like that). A diff in the files with the same hash
is not a finding — Tiled reorders keys and reflows arrays — but it is worth sending: the saved files
then replace the hand-written ones in the tree.

> **Linux half, 2026-10-03** on commit `3bb7d69`, Nobara 44 (GNOME on Wayland, 59.96 Hz, on AC),
> Intel UHD 620. **The gate could not be closed from here, and steps 1-2 are why: there is no
> Tiled on this box.**
> `rpm -q tiled` answers `package tiled is not installed`; the package is in the enabled
> repositories as `tiled.x86_64 1.12.2-1.fc44 nobara`, and flathub carries `org.mapeditor.Tiled`
> 1.12.2. One command installs it:
>
>     sudo dnf install -y tiled
>
> Nothing was installed by this run, and that is not timidity: with no Tiled save there is no file
> to bake, and step 5 asks for the window held next to the Tiled view — two pictures on one screen,
> which is the half of this gate an AI on this box cannot have.
>
> What the run did settle, on the hand-written files exactly as they stand in the tree, so that the
> Tiled pass has a before-value to differ from:
>
> * Step 3 `bash scripts/check_sdk_game.sh --keep` ends `sdk-game: PASS`, with
>   `Release: game.bundle bundle_hash 0x4e7f9ade1d27776a matches bundle.hash` and the same `Debug:`
>   line, each preceded by
>   `neon-rumble: level level1 40x12 tile 16, 2 visual layer(s), 1 texture(s) 384x256`. Five broken
>   fixtures were refused, the MSVC CRT one skipped as it must on gcc. **So the hash in the banner
>   is the hash this tree bakes today**: if the Tiled save moves it, the move is Tiled's, not the
>   importer's drifting under the gate.
> * Step 4 `git diff --stat games/neon-rumble/levels` is empty — which is exactly what it has to be
>   while no Tiled has touched the files.
> * Step 5 runs and exits clean: `./build-sdk-work/game-Release/neon_rumble --frames 120` ended
>   `neon-rumble: window run ok, 120 frames` with exit code 0, above
>   `[gpu] Intel(R) UHD Graphics 620 (KBL GT2) | Vulkan | BC: yes` and
>   `neon-rumble: frame 960x540 zoom 2: 127 sprite(s), 1 run(s), 0 unknown, 0 rejected, 0 dropped`
>   — zoom 2 on this 1x screen, the whole number the text asks for, and no rejected or dropped quad.
>
> **Steps 1, 2 and the picture half of step 5 cannot be judged by any AI on this box,** and the
> reason was measured here, not assumed: the session screen was locked and every output powered
> down — `org.gnome.ScreenSaver.GetActive = (true,)`, `card1-eDP-1/dpms: Off` and the same for
> `DP-1`, `HDMI-A-1`, `HDMI-A-2` — and mutter presents a window to a dead output **once a second**.
> Those 120 frames took 117.8 s, 1.0 a second, against 58 a second for the same binary on a live
> screen earlier the same day (§19 step 7, `--frames 1200` in 20.7 s). On top of that, an AI on
> this box can neither raise nor focus an XWayland window, and `org.gnome.Shell.Screenshot` is
> denied to it. That is finding Н9 of this run, measured a second time here and on a second target.
>
> A practical consequence for whoever runs this next: `gnome-session-inhibit --inhibit idle:suspend`
> does not undo a lock — it keeps the session from going idle, it does not wake a screen already
> blanked. Start the window step on a woken, unlocked screen, or `--frames 600` takes ten minutes
> instead of ten seconds and every picture question comes back unanswerable.
>
> Artefacts: `build/owner-artifacts-linux/g18-tiled-linux.txt` (the three steps and the screen
> state side by side), `g18-sdk-game.txt`, `g18-window.txt`.

> **Windows half, 2026-10-03/04** on commit `5241932`, Windows 11 build 26200, MSVC, Tiled 1.12.2
> from the installer, NVIDIA GeForce MX150 on Vulkan. **This is what closes the gate: steps 1-5 all
> ran, and the picture half of step 5 with them.**
>
> * **Steps 1-2, the save.** `level1.tmj` opened as a 40×12 map of 16 px with the three layers the
>   text names, and both files were saved from that window (JSON, CSV). One thing to carry into the
>   next run: **Save was not greyed out on an untouched document**, so the edit and undo the text
>   prescribes were not needed to reach it — Ctrl+S wrote the file as it stood. If a later Tiled
>   does gate Save on the dirty flag, the edit-and-undo is the way back in.
> * **Step 3, the bake.** `bash scripts/check_sdk_game.sh --keep` ends `sdk-game: PASS` with
>   `Release: game.bundle bundle_hash 0x4e7f9ade1d27776a matches bundle.hash` and the same `Debug:`
>   line, each preceded by
>   `neon-rumble: level level1 40x12 tile 16, 2 visual layer(s), 1 texture(s) 384x256`. **That is
>   the hash the Linux run measured on the hand-written files**, to the bit: the save changed the
>   bytes on disk and not one byte of the bundle.
> * **Step 4, the diff.** `git diff --stat games/neon-rumble/levels` gives
>   `2 files changed, 167 insertions(+), 177 deletions(-)` — `level1.tmj | 239 +++---` and
>   `warped-city.tsj | 105 ++--`. Decoded instead of eyeballed, the saved files differ from the
>   hand-written ones in exactly three things beyond whitespace and key order:
>   1. `tiledversion` `1.10.2` → `1.12.2` in both. The format `version` stays `1.10`.
>   2. `"opacity":1` on each of the three objects of `spawns` (`player`, `street`, `walk`) — a
>      field no fixture ever carried, and the importer ignores it.
>   3. The tileset's image path comes back **escaped**: `"..\/assets\/warped-city\/tileset.png"`.
>      Legal JSON, and the only string in either file with a slash in it, so a hand-written fixture
>      could not have covered the escape by accident. The reader handles it — the texture loads
>      (`1 texture(s) 384x256`) and the facades draw.
>
>   Everything else is formatting: Tiled collapses `{` onto the first key and reflows the CSV rows.
>   Loaded as JSON and compared value by value, the two documents are equal apart from those three
>   fields. The saved files now stand in the tree, as the paragraph above directs.
> * **Step 5, the window.** `build-sdk-work\game-Release\neon_rumble.exe --frames 600` ends
>   `neon-rumble: window run ok, 600 frames` with exit code 0, above
>   `[gpu] NVIDIA GeForce MX150 | Vulkan | BC: yes` and
>   `neon-rumble: frame 960x540 zoom 2: 127 sprite(s), 1 run(s), 0 unknown, 0 rejected, 0 dropped`.
>   Zoom 2 is the whole number the text asks for, and nothing was rejected or dropped.
> * **Step 5, the picture.** The facades and the street are what Tiled draws: the same fronts in the
>   same order over the same street line, the one-way platform and the ladder where Tiled puts them,
>   the queen's feet on the top row of the street. No seam or line between tiles, nothing mirrored,
>   nothing displaced, no shimmer across the pan — and nothing washed out, which was worth checking
>   by name here, because the finding list warns about a doubled colour encoding on a Windows sRGB
>   surface and this is the first time that surface has been looked at.
> * **The violet around the level is the game's own clear colour, not a finding.**
>   `games/neon-rumble/src/rumble_window.cpp:26` sets
>   `color.clearValue = WGPUColor{0.06, 0.02, 0.12, 1.0}`, which is that colour exactly. It reads
>   like a background layer that failed to bake until you look — named here so the next run does not
>   spend an hour on it.
> * **Side by side at full size is not possible on this screen, and that is the screen.** Windows
>   reports 1536×864 to a DPI-aware process on this box; Tiled's Qt layout will not go below about
>   940 logical px of width and the game window is 976, so the two cannot sit unoccluded next to
>   each other. The comparison was made from captures taken seconds apart instead, which answers the
>   same question for a level that does not move — and would not answer the transient one §11 asks.
>
> Artefacts: `build/tiled4.png` (the Tiled view of the saved map), `build/nr2.png` and
> `build/nr3.png` (the window at two points of the pan), `build-sdk-work/game-Release.log` and
> `game-Debug.log`.

## 19. Gate 3 of #24 — boxes and an event out of a real Aseprite

<!-- gate: open | разметить Queen.ase в Aseprite 1.3: slice hit0 на кадрах 3-4, user data тега Death 1:fall; экспорт By Rows с Columns 8 (без счёта колонок лист 2146 px, assetc отказывает), JSON Array, Tags, Slices; bash scripts/aseprite_owner_check.sh <каталог> — четыре строки ok: (hit0 на кадрах 0-1 Death и 0 Hit, fall на кадре 1) и aseprite-export: PASS; тот же экспорт командой из docs/en/guide/aseprite-animations.md во второй каталог — те же четыре ok и PASS; прислать queen.json -->

The importer reads boxes from slices and events from the user data of a tag, and both were tested
on JSON written by hand to the Aseprite 1.3 format. The pack in the tree (`chewbatrij/queen.json`)
is an Aseprite 1.2.8 export with neither, so how 1.3 actually writes a slice key, a slice switched
off on a frame, and the user data of a tag stays unknown until a real Aseprite exports them. The
gate had two halves: the export (steps 1–6) and the window (step 7), where Neon Rumble played the
clips of the same pack from `game.bundle` and F3 drew what the engine thinks a frame is. The window
half was closed by the Linux run of 2026-10-03 below, and since B1e of spec #25 the queen is no
longer in Neon Rumble's manifest: the game draws the Puffolotti fighters, §21 judges them, and
`chewbatrij/queen-rows.*` stays in the tree as an import fixture. Step 7 is kept as the record of
that run, not as a step to repeat. Where the
cel, the boxes and the pivot cross land in pixels, flipped and zoomed, is judged by the pixel golden
`framework_clip_golden` on CI on all three OSes; the window is about the real surface and the real
key.

1. Take `Queen.ase` from the Chewbatrij pack
   (https://opengameart.org/sites/default/files/punchingqueen_gfx.zip) and open it in Aseprite 1.3
   or newer. Tag `Death` spans frames 3 to 8.
2. On frame 3 draw a slice around the fist and name it `hit0`. Move it a little on frame 4. On
   frame 5 remove the slice from that frame only; if Aseprite 1.3 cannot clear one frame of a
   slice, leave it and say so — the box then stays on to the end of the tag, and that is a finding
   about the format, not about the drawing.
3. Open the properties of tag `Death` and type `1:fall` into its user data text.
4. **File → Export Sprite Sheet**: Layout **By Rows** with **Columns = 8**; Sprite — no trim, no
   padding; Borders — leave **Ignore Empty** and **Merge Duplicates** off; Output — **JSON Data**,
   **Array**, with **Tags** and **Slices** ticked, and **Split Layers**/**Split Tags** off. Save the
   sheet as `queen.png` and the data as `queen.json` into one new directory outside the repository.

   **The column count is not a preference, and this step used to omit it.** By Rows with Columns
   left empty lays all 29 frames in one row: 29 × 74 = **2146 px** wide, and `assetc` refuses that
   outright —
   `queen.png: PNG is 2146x75, a texture side is limited to 2048 px`, then `aseprite-export: FAIL`
   with no clip table to look at. Eight columns give **592×300**, which is the sheet size step 7
   expects; any count whose sheet stays under 2048 px on both sides will do.
5. Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window), naming that
   directory:

       bash scripts/aseprite_owner_check.sh <directory>

   The script bakes the export with `assetc`, reads the clip table back out of the bundle with
   `clip_dump` and looks for four rows of it. Clip frame 0 of `Death` is sprite frame 3, and tag
   `Hit` covers the same frame 3, so the box reaches both clips. Expected:

       [assetc] manifest <directory>/queen.bundle (<n> bytes, 2 assets)
       [assetc] bundle_hash = 0x<16 hex digits>
         ok: queen/Death frame=0 boxes=hit0 event=-
         ok: queen/Death frame=1 boxes=hit0 event=fall
         ok: queen/Death frame=2 boxes=- event=-
         ok: queen/Hit frame=0 boxes=hit0 event=-
       aseprite-export: PASS

   A `missing:` row is printed with every `Death` and `Hit` row of the table under it, and the
   script ends with `aseprite-export: FAIL`.

   Then export the same marked file from the command line, with the command
   [Animations in Aseprite](en/guide/aseprite-animations.md) shows, into a second new directory, and
   run the script on it. No runner has Aseprite, so this step is the only check of that command:

       aseprite -b Queen.ase --sheet <directory 2>/queen.png --sheet-type rows --sheet-columns 8 --data <directory 2>/queen.json --format json-array --list-tags --list-slices
       bash scripts/aseprite_owner_check.sh <directory 2>

   Expected: the same four `ok:` rows and `aseprite-export: PASS`. An unknown flag, a sheet that is
   not 592×300 or a missing row is a finding about the page, and the page changes in the commit that
   records it.
6. Send back `queen.json` and the output of step 5. The JSON goes into the tree as a fixture that
   pins the hit box to frames 3 and 4 and the event `fall` to frame 4.
7. *Closed 2026-10-03 on Linux, retired by B1e of spec #25 — the build it describes no longer
   draws the queen.* Build the game against the SDK and run the window from the same shell (Windows: from the
   `scripts\win-dev.bat shell` window, with `.exe` and backslashes):

       bash scripts/check_sdk_game.sh --keep
       ./build-sdk-work/game-Release/neon_rumble --frames 1200

   The gate prints, for each configuration,
   `neon-rumble: fighter 71 clip(s), sheet 592x300, spawn 200,224 facing right`. In the window the
   queen stands on the left of the street with her feet on its top line, facing right, and
   plays Walk for 60 frames, then Jab, Hook and Uppercut for 30 frames each; every second round
   of 150 frames she faces left. Both the showcase and the camera step once per drawn frame, and
   the window presents at the display rate: one second for Walk at 60 Hz, half a second on a
   120 Hz ProMotion screen — that is not a finding. Since B7b the camera is held by the level's
   `bounds` and pans only 48 map pixels right and back, so she stays in view the whole run, on a
   1x screen and a 2x one alike. Press **F3**: a cyan frame around her cell
   (74×75 sheet pixels, larger than her silhouette) and a white cross at her feet; **F3** again
   hides them. The pack has no slices, so no coloured boxes appear — red hit, green hurt and yellow
   push frames show up once a sheet with boxes is in the bundle. Send back a screenshot with the
   overlay on.

What counts as a finding: a refusal (it names `queen.json:<line>:<column>` and what to change in
the export), a `queen.json` whose tag `Death` has no `data` field after step 3 — the user data did
not reach the export, and events then move to the fallback of slices `ev:<name>` — or a slice that
Aseprite would not clear on one frame. In the window: the queen floating above the street or sunk
into it, sliding against the street while the camera pans, a cell cut off by its frame (a
neighbour's limb at an edge means a wrong cell rectangle), the cross away from her feet, the
overlay not following a flip, or F3 doing nothing.

### Step 7 on Linux 2026-10-03 (Nobara, Intel UHD 620, GNOME/XWayland): the window half passes

Steps 1-6 stay blocked: Aseprite is in neither the Nobara repositories nor any configured flatpak
remote, and building it from source is not something this run was allowed to do. Step 7 was run on
its own, because it reads the pack already in the tree and says nothing about the export.

`bash scripts/check_sdk_game.sh --keep` ended `sdk-game: PASS`, both configurations printing
`bundle_hash 0x4e7f9ade1d27776a`, and `./build-sdk-work/game-Release/neon_rumble --frames 1200`
ended `neon-rumble: window run ok, 1200 frames` with exit 0 in 20.7 s -- 58 frames a second against
a 59.96 Hz screen, so the 1x frame numbers of the text apply unchanged. Above it the gate's line,
verbatim: `neon-rumble: fighter 20 clip(s), sheet 592x300, spawn 48,160 facing right`, and
`[gpu] Intel(R) UHD Graphics 620 (KBL GT2) | Vulkan | BC: yes`.

What the screen showed, measured off the captures in `build/owner-artifacts-linux/`:

* The cyan frame is **148x150 screen pixels** at zoom 2 -- **74x75 sheet pixels**, the number the
  text asks for -- and it stands clear of her silhouette on every side.
* The white cross sits at **x = the frame's horizontal centre, y = 398** in every capture, and 398
  is the first row of the street: her feet are on its top line, neither floating nor sunk.
* **F3 again hides both.** The control is `g19-f3-off-queen-visible.png`: the queen is in view,
  pure `#00FFFF` count is 0 and the cross is gone. The level never paints pure cyan itself, so that
  count is a clean yes/no for the overlay.
* **The overlay follows the flip.** She faces left on the pass that covers frames ~235-300 and
  right from ~300 (`g19-f3-left.png`, `g19-f3-right.png`): across that boundary the frame keeps
  moving 10 px per 95 ms sample with no sideways jump, and the cross stays on the frame's centre.
* **No coloured boxes.** Scanned every capture for pure red, green, yellow, magenta, orange and
  blue: none, as the text expects of a pack without slices.
* **She does not slide.** Cropping the cell by its own frame and differencing consecutive captures
  leaves 1.5-2.3 k changed pixels -- the clip is playing while she travels, not a still sprite
  dragged along.

Captures: `g19-f3-right-full.png` (the whole window, overlay on), `g19-f3-left-zoom.png` (4x on the
cell, facing left), plus the two bursts `g19b/` and `g19c/` the numbers were read from.

**Not a defect of the engine, but it costs an hour if it is not known.** On GNOME a blanked screen
throttles mutter's compositing to about 1 fps, and an XWayland client then crawls: `neon_rumble`
printed `surface texture status 1 - frame skipped`, sat in `drm_syncobj_array_wait_timeout` at 0%
CPU and advanced eight pixels in four seconds, which reads exactly like a hung GPU. The screen was
blanked -- `org.gnome.ScreenSaver.GetActive` answered `(true,)`. Run window gates with

    gdbus call --session --dest org.gnome.ScreenSaver --object-path /org/gnome/ScreenSaver \
        --method org.gnome.ScreenSaver.SetActive false
    gnome-session-inhibit --inhibit idle:suspend --reason "owner gate run" <command>

and the same binary finishes 1200 frames in 20.7 s. The gate stays **open** on steps 1-6.

### Steps 4-6 rehearsed on Windows 2026-10-03/04: the script works, and step 4 was wrong

Aseprite 1.3 was built from source on the Windows box (`C:\Users\n0sfe\_dev\gate19`, Skia from the
pinned release, `build/bin/aseprite.exe` reporting `1.3.18.6-dev`) and `Queen.ase` from the
Chewbatrij pack was exported through it headlessly. **Steps 1-2 and 7's screenshot still want your
mouse** — a slice is drawn, not scripted, and the frame-5 key below was written by hand rather than
cleared in the editor. What the rehearsal did settle is everything that would have cost you a
drawing session to find out:

* **Step 4 was wrong, and it is corrected above.** By Rows with no column count is one row of 29
  frames, 2146 px wide, and the bake dies on it before any clip table exists:
  `[assetc] …/queen.manifest:1: …/queen.png: PNG is 2146x75, a texture side is limited to 2048 px`
  then `aseprite-export: FAIL`. With **Columns = 8** the sheet is 592×300 — the number step 7 reads
  back — and the same export bakes clean.
* **The user data of a tag does reach a real export.** Set through the Lua API
  (`tag.data = "1:fall"`, which is the same field the properties dialog writes), it comes back in
  `queen.json` as `{"name":"Death","from":3,"to":8,…,"data":"1:fall"}`. So the finding the text
  warns about — a `Death` with no `data` after step 3 — is not what 1.3.18 does, and the
  `ev:<name>` slice fallback is not going to be needed for this reason.
* **The script itself is proved end to end.** With the hand-written `hit0` keys added to that real
  export, `bash scripts/aseprite_owner_check.sh <dir>` prints
  `[assetc] bundle_hash = 0x344dde92fd4dc61a` and the four rows exactly as step 5 predicts them:
  `ok: queen/Death frame=0 boxes=hit0 event=-`, `frame=1 boxes=hit0 event=fall`,
  `frame=2 boxes=- event=-`, `ok: queen/Hit frame=0 boxes=hit0 event=-`, then
  `aseprite-export: PASS`. If your own export fails, the failure is in the export, not in the
  script.
* **What the slice half still does not know.** The keys fed to the bake were
  `{"frame":3,"bounds":{20,30,10,12}}`, `{"frame":4,…}` and
  `{"frame":5,"bounds":{"x":0,"y":0,"w":0,"h":0}}` — `assetc` accepts that shape and drops the box
  from frame 2 of the clip, which is the behaviour step 5 asks for. But **a zero-bounds key is a
  guess at how Aseprite writes a slice switched off on a frame**; it may write no key at all, or
  omit the frame from `keys`. That is exactly the question step 2 exists to answer, and only the
  editor can.

## 20. Gate 7 of #24 — the street of Neon Rumble on a real screen

<!-- gate: closed 2026-10-07 -->

> **Closed 2026-10-07** on macOS by the owner, with Tiled 1.12.2, on a build of `0739aa9`. Step 2 is
> now on record: `level1.tmj` was saved at 12:59 and `warped-city.tsj` and `neon-signs.tsj` later the
> same day, each from Tiled, and `git diff --stat` after the saves held one line only: Tiled drops the
> final newline of `neon-signs.tsj`, which the two other hand-written files never had. That is the
> formatting this section says is not a finding, and the saved tileset is committed as Tiled wrote
> it. `check_sdk_game.sh` passed in Release and Debug with `bundle_hash 0x4e5006e2c1647d5d`.
> Steps 1 and 3–6 stand from the run below and the window of the same day: the parallax, the signs,
> 21:9 and 4:3 and the credits came out as predicted. The sky of level 1 that the window showed above
> the far city is kept as drawn by the owner's decision (`.context/notes/25-sky.md`).

> **Run 2026-10-06 on macOS** (owner, build of `f1e0b61`, `bundle_hash 0xd69efa7c7e0f7a6c` at the
> time): the owner reported steps 1–6 as predicted, in one line, with no screenshot of the 21:9 and
> 4:3 windows or the credits pages and no `git diff --stat` of step 4. Whether the files were saved
> by Tiled in step 2 is not on record, and the save is the point of this gate, so it stays open until
> that is confirmed. The hash in step 3 has moved since: the fix of the §21 finding shifted the depth
> band and the spawns of `level1.tmj` and set the fighters' pivot, and B2a of #25 marked the hit and
> hurt boxes on the Puffolotti sheets, so a re-run expects `0x4e5006e2c1647d5d`.

B8 gave level 1 three parallax layers and two animated signs (B8a) and gave the game a bitmap font
and a credits screen (B8b). CI already holds what a runner can see: the bake walks the camera over
every position inside `bounds` and proves that each layer covers the widest view, the pixel golden
draws parallax and a tile animation on two ticks, and `--headless` prints the counts of the credits
layout with `0 unknown` and `0 dropped`. Four things are left. They need a real window, a real
resize, real font pixels and a real Tiled:

- **The y axis of `far-city` and `near-city`.** Both layers have `cover_y = false`, so the bake
  checks them on x only. Whether their bottom edges stay hidden behind the facades in a tall window
  is checked by nobody but this scenario.
- **Motion.** A golden is one frame. Shimmer, a seam at the repeat of a layer, or a sign that
  stutters shows only while the camera moves and the frames follow each other.
- **The glyphs.** `0 unknown` says that every character has a glyph. It does not say that the glyph
  is the right letter, or that the wrapped URL reads correctly.
- **The save.** §18 closed on a level without image layers with properties and without a second
  tileset with `animation`. `level1.tmj` and `neon-signs.tsj` are written by hand to the Tiled 1.10
  format, and only a real Tiled shows what it writes for them.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Open `games/neon-rumble/levels/level1.tmj` in Tiled 1.10 or newer. The map is 40×21 tiles of 16
   px. Its layers are, from the back: image layers `sky`, `far-city` and `near-city`, tile layers
   `facade`, `signs` and `street`, and the object layer `spawns`. With **View → Show Tile
   Animations** on, the two signs on the facades animate in the map view.
2. Move any tile, undo it, and save the map with **File → Save** (keep JSON, keep CSV layer format).
   Open `neon-signs.tsj` from the Tilesets panel and save it too.
3. Run the gate:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS`, the lines
   `Release: game.bundle bundle_hash 0x4e5006e2c1647d5d matches bundle.hash` and the same for
   `Debug`, each run preceded by
   `neon-rumble: level level1 40x21 tile 16, 6 visual layer(s), 16 animated tile(s), 5 texture(s) 384x256 128x128 128x312 144x124 493x209`.
4. Send back `git diff --stat games/neon-rumble/levels` and, if the gate failed, its FAIL line.
5. Start the window with no frame limit, keep Tiled open next to it, and close the window yourself
   when the steps below are done:

       ./build-sdk-work/game-Release/neon_rumble

   **Parallax.** Walk Banderas, the fighter on the left, with the arrows or WASD: the camera
   follows him and stops at the level's `bounds`. `sky` stands still, `far-city` moves
   at a quarter of the camera, `near-city` at half, and the facades and the street move with the
   camera. No layer shimmers, jumps by a pixel against its neighbours, or shows a seam where its
   image repeats. **Signs.** Coca-Cola on the left plays 3 frames of 200 ms, the neon banner on
   the right plays 4 frames of 150 ms, and both look and time like the animation in Tiled.
6. Resize the window, by dragging, to about **21:9** (for example 1680×720 points) and then to about
   **4:3** (for example 960×720 points). At 21:9 the view opens up along x: more of the city on
   both sides. At 4:3 it opens up along y. The view never opens past 576×312 world pixels. What
   lies beyond that is a band of the map colour `#0b0f1a` (dark blue). On a 1x screen a 960×720
   window has such bands, 48 px tall, above and below. A 2x screen at the same size shows 384×288
   of the world and no bands. In both shapes the world inside the bands has no holes. The sky shows
   wherever the two cities do not reach, and the bottom edge of `far-city` or `near-city` never
   shows as a straight line above the facades. Send back a screenshot of each shape.
7. Press **F1**. The level dims, the title `Credits · Титры  1/2` stands at the top in cyan, and
   under it, in white, the packs appear in alphabetical order, starting with `chewbatrij`, each as
   "pack — author, license" with its URL below. Long URLs wrap to the next line instead of running
   out of the play area. Not a single `?` stands in for a letter. The Cyrillic of the title, `·`,
   `—`, `í` and `é` are all in the font. In the 21:9 window the text stays inside the central play
   area. Press **F1** again: the title reads `2/2`, the page ends with `warped-city`, and no pack
   is split between the two pages; together they show all five packs `chewbatrij`, `monogram`,
   `puffolotti-bad-company`, `puffolotti-up2` and `warped-city`. A third **F1** hides the screen.
   Send back a screenshot of each page.

   > **Step 7 confirmed 2026-10-06 on macOS** (owner, build of `f61b3bc` from
   > `bash scripts/check_sdk_game.sh --keep`): F1 opens page `1/2`, the second press shows `2/2`,
   > the third hides the credits, as predicted. Steps 1–6 of this scenario stay open.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit
code is 0.

What counts as a finding: an import refusal (it names `level1.tmj:<line>:<column>` or
`neon-signs.tsj:<line>:<column>` and what to change in Tiled), a different `bundle_hash`, a layer
that shimmers or shows a seam, a sign whose timing differs from Tiled, a hole or a layer edge in
the 21:9 or 4:3 window, a band of any colour but `#0b0f1a`, a `?` or a wrong letter in the credits,
a URL cut at the edge, and any `neon-rumble: tick <n>: credits …` line in the terminal. A diff in
the saved files with the same hash is not a finding, because Tiled reorders keys and reflows arrays.
It is worth sending all the same: the saved files then replace the hand-written ones in the tree.

## 21. Gate 1 of #25 — three fighters walk the street in depth

<!-- gate: closed 2026-10-06 -->

> **Closed 2026-10-06** on macOS by the owner, on a build of `71727a8`, the fix of the run below.
> `check_sdk_game.sh --keep` passed in Release and Debug with `bundle_hash 0x785987a3c33db92b` and
> the brawl lines `tick 0: hash 12e8ec6a8b655020` and `tick 60: hash f0682e033ec3cf0d`. Steps 1–6
> were run again and came out as predicted: the three fighters stand with their feet on the pavement
> below the facades, the F3 cross sits at the feet, the arrows, WASD, Space and K move Banderas in
> the band 232..264, the nearer fighter covers the farther one and the order flips around Rainbird
> and Adler. The verdict of step 7 from the first run stands: the 3D-rendered Puffolotti next to
> the pixel street is accepted for the prototype.

> **Run 2026-10-06 on macOS** (owner, build of `f1e0b61`): everything but the feet was reported as
> predicted, and the style of step 7 is accepted for the prototype. Finding: the three fighters stood
> in the air in front of the facades. The `walk` band lay on facade rows 12–13 (z 192..224), not on
> the pavement (rows 14 and below), and the pivot sat at the bottom of the cell, 7–9 px under the
> feet, and 25–41 px under them on the top frames of `jump`. The fix moves the band and the spawns by
> +40 in z and puts a `pivot` slice on the feet line of every clip and of each airborne frame of
> `jump`. Steps 1–6 are to be run again on the fixed build; the verdict of step 7 stands.

B1 of spec #25 put three Puffolotti fighters into Neon Rumble: Banderas under the keyboard, Rainbird
and Adler standing to his right at different depths, both facing left. CI holds the simulation:
`--headless` drives Banderas by a script for 60 ticks and pins the state hash, the positions and the
draw order before and after, and the crossplay job compares the brawl scenario hashes across the
three OSes. Three things are left that only a screen answers:

- **The keys.** The script feeds `move_x` and `move_z` directly. Whether the arrows, WASD, Space and
  K reach the same input is seen only in a window.
- **Depth on screen.** The draw order is a sorted list in the log. Whether the nearer fighter really
  covers the farther one, and whether a jump lifts the sprite off its line instead of moving it in
  depth, is a picture.
- **The style.** Puffolotti is a 3D render scaled to pixels, Warped City is drawn pixel art. The
  spec took the mismatch as an accepted risk, to be judged on a live frame of level 1. That frame is
  this scenario.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS`, `bundle_hash 0x4e5006e2c1647d5d` in Release and Debug, and in each
   run the lines

       neon-rumble: brawl tick 0: hash 12e8ec6a8b655020, banderas 200,264 y 0, rainbird 264,240 y 0, adler 328,252 y 0, draw rainbird adler banderas
       neon-rumble: brawl tick 60: hash f0682e033ec3cf0d, banderas 320,234 y 32, rainbird 264,240 y 0, adler 328,252 y 0, draw banderas rainbird adler

2. Start the window and close it yourself when done:

       ./build-sdk-work/game-Release/neon_rumble

   Three fighters stand on the street: Banderas on the left facing right, Rainbird and Adler to his
   right facing left, Rainbird higher on the screen (farther away) than Adler. All three stand with
   their feet on the pavement below the facades, not in the air in front of them.
3. **Walking.** Left and right arrows (or A and D) walk Banderas along the street; up and down (W and
   S) move him away and towards you, up the screen and down it. He stops at the edges of the strip,
   104 to 536 along the street and 232 to 264 in depth, and never leaves the street. The camera
   follows him and stops where the level's `bounds` end; in a window at least as large as the
   384×216 zone he stays in view along the whole strip. A window smaller than the zone is cropped
   (the terminal says so), and there he may leave the frame near the ends of the strip — not a
   finding. The simulation steps once per drawn frame, so on a 120 Hz screen he walks and jumps
   twice as fast as on a 60 Hz one — not a finding either.
4. **Depth order.** Walk him behind Adler (up, then right) and in front of him (down, then right).
   Whoever is nearer, lower on the screen, is drawn on top. Do the same with Rainbird.
5. **Jump.** Space or K jumps once per press; holding the key does not jump again after landing.
   Without up or down held, he lands on the line he jumped from. In the air the arrows still steer
   him along the street and in depth, so with up or down held he lands on another line.
6. **F3** shows the cell frame and the pivot cross on all three fighters; the cross stays at the
   feet, and during the jump it rises with the sprite.
7. **Style.** Look at the fighters against the facades and the street: the render of Puffolotti next
   to the pixel art of Warped City. Send back a screenshot with all three in view and a word on
   whether the mismatch is acceptable for the prototype. A no is not a defect of the engine; it
   reopens the choice of pack made in step 0 of the spec.

What counts as a finding: a different hash or position in step 1, a key that does nothing or moves
the wrong axis, a fighter leaving the strip or the street, the camera losing him in a window not
smaller than the zone, a farther fighter drawn over a nearer one, a second jump from a held key, a
landing on another line than the take-off with up and down untouched, a fighter floating above or
sunk into the street, and the overlay not following the sprite.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 22. Gate 2 of #25 — strikes, teams and the F3 depth bands

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки brawl/hit из шага 1; окно neon_rumble: J jab, U cross, L kick, J в прыжке jump_kick попадают в adler (стоп-кадр, отброс, строка hit с hp), удар сквозь rainbird без урона и без строки, F3 — красная рамка hit0 на активных кадрах и полосы z на полу; сверить кадр kick 54 и кадры cross 46–47; прислать скриншот F3 с jab в adler -->

B2d of spec #25 moved Neon Rumble from walking to `step_brawl`: the fighter tables in
`games/neon-rumble/fighters/*.fighter` are baked into `game.bundle`, Banderas strikes, Adler is a
dummy of team 1 and Rainbird an ally of team 0, both without input. CI holds the simulation:
`--headless` drives Banderas by a script for 240 ticks — jab, kick, a walk to the knocked-back Adler,
a jump kick, another walk, a cross and a jab — and pins the hits, Adler's reactions, the hp left and
the state hash before and after. Four
things are left that only a screen answers:

- **The keys.** The script feeds the strike directly. Whether J, U and L reach it, once per press,
  is seen only in a window.
- **The hit on screen.** The log says who hit whom and for how much. Whether the stop frame and the
  knock-back read as a hit, and whether the red `hit0` frame lies on the fist or the foot, is a
  picture.
- **The team filter.** The script never puts Rainbird inside a strike; a jab through him is tried by
  hand.
- **The windows of B2a.** The active frames of `hit0` were chosen by eye from the sheets; the last
  frame of `kick` and the tail of `cross` are disputed and are judged here.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS`, `bundle_hash 0x6afe949291c3d9b2` in Release and Debug, and in each
   run the lines

       neon-rumble: brawl tick 0: hash 7ff578b61c84afda, banderas 200,264 y 0 hp 100, rainbird 264,240 y 0 hp 100, adler 328,252 y 0 hp 100, draw rainbird adler banderas
       neon-rumble: hit tick 53: banderas/jab -> adler, damage 6, hp 94
       neon-rumble: react tick 53: adler hurt
       neon-rumble: hit tick 66: banderas/jab -> adler, damage 6, hp 88
       neon-rumble: hit tick 75: banderas/cross -> adler, damage 8, hp 80
       neon-rumble: react tick 92: adler stand
       neon-rumble: react tick 101: banderas block
       neon-rumble: react tick 111: banderas stand
       neon-rumble: hit tick 154: banderas/jump_kick -> adler, damage 10, hp 70
       neon-rumble: react tick 154: adler fall
       neon-rumble: react tick 174: adler down
       neon-rumble: hit tick 198: banderas/jump_kick -> adler, damage 10, hp 60
       neon-rumble: react tick 209: adler getup
       neon-rumble: react tick 239: adler stand
       neon-rumble: hit tick 329: banderas/run_kick -> adler, damage 12, hp 48
       neon-rumble: react tick 329: adler fall
       neon-rumble: react tick 346: adler down
       neon-rumble: react tick 353: banderas dodge
       neon-rumble: react tick 376: adler getup
       neon-rumble: react tick 377: banderas stand
       neon-rumble: react tick 406: adler stand
       neon-rumble: hit tick 417: banderas/grab -> adler, damage 6, hp 42
       neon-rumble: react tick 417: adler thrown
       neon-rumble: react tick 436: adler down
       neon-rumble: react tick 466: adler getup
       neon-rumble: brawl tick 480: hash b82f61c720937664, banderas 458,252 y 0 hp 100, rainbird 264,240 y 0 hp 100, adler 413,252 y 0 hp 42, draw rainbird banderas adler

   U on tick 200 makes no line: Adler lies until tick 209, and since B3f a lying fighter is hit only
   by a strike with `hits_down` (§27) — the cross has none. Falling, lying and rising are judged in
   §23.

   Since B3c the script presses J on ticks 44, 56 and 68 and gets the chain of Banderas — jab, jab,
   cross, judged in §24 — instead of a jab and a kick. The kick on the ground is no longer in the
   script: it pushes Adler out of reach of the jump kick until he could not get up within 240 ticks.
   L is judged by hand in step 4.

   Since B3d the script runs 360 ticks: Banderas walks back to the left from tick 244, taps right on
   ticks 304 and 306, runs, and presses L on tick 312. Kick on the run is `run_kick`, which rolls on
   with the run and launches Adler; it is judged in §25.

   Since B3e the script holds I on ticks 100–109 (the block, `banderas block` then `banderas stand`)
   and presses O on tick 352 (the roll past the fallen Adler); both are judged in §26.

   Since B3f the script runs 480 ticks. The jump kick of tick 198 hits Adler where he lies (`jump_kick`
   has `hits_down`): no reaction line, and his getup moves from 204 to 209 by the hitstop. The jab of
   tick 238 is gone — Adler is still rising. Banderas walks left on ticks 378–390 and presses H on
   tick 408: the grab lands on 417 and Adler flies thrown, lies on 436 and rises on 466. Grab, throw
   and the hit on the ground are judged in §27.

2. Start the window from a terminal you can read, and close it yourself when done:

       ./build-sdk-work/game-Release/neon_rumble

   Walking, depth and the jump are those of §21 and are not re-judged here.
3. **Jab.** Walk Banderas to Adler on his line (Adler's feet level with his) and press J. Banderas
   jabs once; holding J does not jab again. When the fist reaches Adler, both freeze for a moment
   (the stop frame), then Adler is pushed a little to the right on his hurt pose, and the terminal
   prints

       neon-rumble: hit tick <n>: banderas/jab -> adler, damage 6, hp <hp>
       neon-rumble: react tick <n>: adler hurt

   with hp 6 lower each time — wait for each jab to end before the next J: since B3c a J pressed
   while a jab that landed is still out goes on along the chain, and the third is a cross (§24).
   One jab hits once, however long the fist stays on him.
4. **Cross and kick.** U throws a cross: the jab's longer twin, damage 8, and its line says
   `banderas/cross`. L kicks: a longer move with a farther reach, damage 12 and a longer push. U and
   L in the air do nothing. Since B3c a key pressed while a strike is still playing waits for it for
   up to 8 ticks, and J after a jab that landed goes on along the chain — §24 judges both. Two keys
   one move, in the order J, U, L: J with U or L gives the jab, U with L the cross, and a strike key
   together with Space gives the strike on the ground without the jump — a fighter striking on the
   ground stays on it. Neither is a finding.
5. **Jump kick.** Space or K, then J in the air: `jump_kick`. On a hit Adler is thrown up and back
   and lands on his line; the line in the terminal says `banderas/jump_kick`, damage 10. Since B3b he
   then lies and gets up instead of landing on his feet — §23 judges that. Since B3a
   the pivot of `jump_kick` sits on the feet of each frame, as that of `jump` does: when the jump
   turns into the kick, the sprite does not hop up or down, the feet go on along the arc of the
   jump, and the fighter lands with his feet on the shadow. A J pressed late in the jump, while the
   first `jump_kick` still plays, waits in the buffer and may start a `jump_kick` on the ground after
   landing: the move is chosen at the press, not at the start. That is a known edge left to B3d, not
   a finding.
6. **Teams.** Walk to Rainbird on his line and jab, cross and kick him. The strike goes through him: no stop
   frame, no push, no line in the terminal. Rainbird is team 0, as Banderas.
7. **F3.** On all three: the cell frame, the green `hurt0` frame, the pivot cross, and on the floor
   under each fighter a green band — the thickness of his body in depth, ±6. During a strike, on its
   active frames only, a red `hit0` frame appears on the fist or the foot (for `jump_kick` — on the
   foot, which B3a lowered together with the sprite), and a red band ±8 under
   it. A hit lands only where the red frame overlaps Adler's green one and the bands overlap.
8. **The disputed frames.** Watch the red frame of `kick` on its last active frame (sheet frame 54):
   is the foot still out, or already coming back? Send a word: keep it or cut it. Then the cross:
   its red frame is on sheet frames 43–45 and gone on 46–47, where the arm still reaches forward and
   up. Send a word: the window ends at 45, or it runs on through 46–47.

Adler's hp stops at 0 and he keeps standing: there is no knock-out in B2 — not a finding. As in §21,
on a 120 Hz screen everything runs twice as fast.

What counts as a finding: a different hash, hit line or position in step 1; J, U or L that does nothing
or strikes twice on one press; a hit without a line in the terminal or a line without a visible hit;
a strike that hits Adler while the red and green frames do not touch, or misses while they overlap
on one line; damage or hp different from the steps; any hit on Rainbird; a red frame outside the
active frames or away from the fist or foot; bands that do not follow the fighter or sit off his
feet.

Send back: a screenshot with F3 on during a jab into Adler, the terminal lines of steps 3–5, and the
two verdicts of step 8.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 23. Gate 3 of #25 — Adler falls, lies and gets up

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки react tick 154 fall, 174 down, 204 getup, 234 stand; окно neon_rumble с F3: jump_kick (Space, J в воздухе) в adler — полёт, лёжа, подъём; крест пивота и ноги на тени на каждом кадре fall/down/getup; удары J/U/L в лежачего и встающего — без строки hit; прислать скриншоты F3 лёжа и на подъёме -->

B3b of spec #25 gave the fighters reactions: a light or heavy hit puts the target on its `hurt` clip
for the hitstun of the strike, a `launch` hit — `jump_kick` — throws it into `fall` until it lands,
then `down` for the `down` ticks of its `.fighter` (30) and `getup` for the `getup` ticks (30), with
no hit landing on it in any of the three. B3a moved the pivot of every airborne and lying frame of
`fall` and `down` to the feet of that frame; until now the game never played those clips, so where
the sprite sits on them is judged only here. Banderas and Rainbird have the same clips, but nothing
in the game strikes them yet — only Adler is judged.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS` and, in each run, among the lines of §22 step 1,

       neon-rumble: react tick 154: adler fall
       neon-rumble: react tick 174: adler down
       neon-rumble: react tick 204: adler getup
       neon-rumble: react tick 234: adler stand

2. Start the window from a terminal you can read and press **F3**:

       ./build-sdk-work/game-Release/neon_rumble

3. **The fall.** Walk Banderas to Adler on his line, jump (Space or K) and press J in the air. On the
   hit Adler is thrown up and back on his `fall` clip; the terminal prints `react tick <n>: adler
   fall`. Watch the pivot cross and the feet: on every frame of the flight the cross stays at the
   feet, and the sprite does not jump up or down against it between frames.
4. **Lying.** Where he lands he lies on `down` for half a second (`react … adler down`). His body is
   on the pavement — not floating above his shadow and not sunk into it — and the cross is where his
   feet are.
5. **Getting up.** Then `getup`, half a second (`react … adler getup`), and he stands idle
   (`react … adler stand`). The feet stay on the shadow through the whole rise.
6. **Nothing hits him down.** While he flies, lies or rises, jab, cross and kick him. No stop frame,
   no push, no `hit` line. Once he stands, a jab hits again with a `hit` line and `react … adler
   hurt`.

What counts as a finding: different `react` lines in step 1; a frame of `fall`, `down` or `getup`
whose feet are off the shadow by more than a few pixels, or a sprite that hops between two frames
of one clip; Adler standing up before the `getup` line or staying down after the `stand` one; any
`hit` line while he is down or rising.

Send back: a screenshot with F3 on while Adler lies, one while he gets up, and the terminal lines of
steps 3–6.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 24. Gate 4 of #25 — the chain and the buffer

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки hit tick 53 jab, 66 jab, 75 cross; окно neon_rumble: J J J в adler — jab, jab, cross с одной строкой hit на каждый; J мимо — снова jab; J во время удара ждёт его конца; U после jab цепочку не продолжает; прислать строки hit и слово о темпе цепочки -->

B3c of spec #25 gave the fighters a chain and an input buffer. Each `.fighter` names its chain in
the `chain` row, the last move being the finisher: Banderas `jab jab cross`, Rainbird `jab jab cross
kick`, Adler `jab jab kick`. J during a jab that has landed goes on to the next move of the chain
once the jab reaches its `cancel` frame (frame 2 of `jab`, frame 1 of `cross`); a jab that misses
leaves no chain, and the next J is a jab again. A strike key pressed while the fighter cannot strike
yet — a strike, a hurt, the stop frame — waits for up to `buffer` ticks (8), not counting the stop
frame, which freezes the buffer with the fighter, and starts as soon as it can. Only Banderas is under the keys; the chains of Rainbird and Adler are pinned by the tests.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS` and, in each run, among the lines of §22 step 1,

       neon-rumble: hit tick 53: banderas/jab -> adler, damage 6, hp 94
       neon-rumble: hit tick 66: banderas/jab -> adler, damage 6, hp 88
       neon-rumble: hit tick 75: banderas/cross -> adler, damage 8, hp 80

   The script presses J three times; the third J gives the cross, the finisher of Banderas.

2. Start the window from a terminal you can read:

       ./build-sdk-work/game-Release/neon_rumble

3. **The chain.** Walk Banderas to Adler on his line and press J three times, one press after the
   other, not too fast. Banderas throws jab, jab, cross: three `hit` lines, `banderas/jab`,
   `banderas/jab`, `banderas/cross`, with damage 6, 6, 8. A fourth J starts a jab again.
4. **The miss.** Step back out of reach and press J three times. Three jabs, no cross: a jab that
   hits nothing does not chain.
5. **The buffer.** Next to Adler, press J and press it again at once, while the first jab is still
   out. The second press is not lost: the second jab follows when the first reaches its cancel frame,
   or when it ends. A press waiting longer than 8 ticks (the stop frame not counted) before the
   fighter is free is dropped — that is the buffer, not a finding.
6. **Off the chain.** Next to Adler, press J and then U. The cross waits for the jab to end and comes
   out on its own: it does not continue the chain, and the next J starts it over.

What counts as a finding: different `hit` lines in step 1; a cross out of two jabs that missed; a
fourth move after the cross; a J pressed during a jab that does nothing; a chain that waits so long
that it reads as two separate strikes, or is so quick that the jab never shows — send a word on the
pace either way. A `jump_kick` on the ground after a late J in the air is the known edge of §22
step 5, not a finding.

Send back: the terminal lines of steps 3–6 and one word on the pace of the chain.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 25. Gate 5 of #25 — the run and the kick from it

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки hit tick 329 banderas/run_kick, react tick 329 adler fall; окно neon_rumble: двойной тап вправо — бег, отпускание/разворот/удар гасят бег, медленный двойной тап — шаг; L на бегу — run_kick катится в adler и валит его; прыжок с бега летит с той же скоростью; прислать строки hit/react и слово о темпе двойного тапа -->

B3d of spec #25 gave the fighters a run. A second press of a direction in the same direction within
`run_tap` ticks of the first (12 for Neon Rumble, 0.2 s) starts a run on the ground at `run_x` (3
against the walk's 2). The run lasts while the direction is held and ends on release, on a reversal,
on a strike and on a hurt; a jump from the run keeps its speed in the air. L on the run is
`run_kick`: a row of the move table on the `kick` clip that rolls the fighter on at `run_x` while
it plays, and launches what it hits. J and U on the run are the usual jab and cross, and they stop
the run. The run plays the walk clip, only faster — a clip of its own is not part of B3d.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS` and, in each run, among the lines of §22 step 1,

       neon-rumble: hit tick 329: banderas/run_kick -> adler, damage 12, hp 52
       neon-rumble: react tick 329: adler fall
       neon-rumble: react tick 346: adler down

   The script walks Banderas back, taps right twice and presses L on the run, 17 ticks before the
   kick reaches Adler: a kick from a stand at that distance misses, the rolling one does not.

2. Start the window from a terminal you can read:

       ./build-sdk-work/game-Release/neon_rumble

3. **The double tap.** Tap right twice quickly. Banderas runs: visibly faster than the walk. Let go —
   he stops; press right once — he walks. Tap right twice slowly (more than a fifth of a second apart)
   — he walks. Run and press left — he turns and walks back. The same to the left.
4. **The kick on the run.** Step back from Adler on his line, run at him and press L before you reach
   him. Banderas kicks and keeps rolling on through the kick; the terminal prints

       neon-rumble: hit tick <n>: banderas/run_kick -> adler, damage 12, hp <hp>
       neon-rumble: react tick <n>: adler fall

   and Adler falls, lies and gets up as in §23. L from a walk or a stand is the plain `kick`
   (`banderas/kick`, Adler hurt, not fallen), and Banderas does not roll.
5. **J and U on the run.** Run and press J: a plain jab, the run stops and Banderas stands while it
   plays; the same for U and the cross. An L pressed during that jab waits for it and comes out as the
   plain `kick`: the run ended with the jab.
6. **The jump from the run.** Run and jump: Banderas flies as far as the run carries him, farther
   than a jump from the walk.

What counts as a finding: a run from one press; a run from two presses in different directions; a run
that goes on after a release or a reversal; `banderas/kick` on the run or `banderas/run_kick` off it;
a run kick that stands still; a double tap that needs to be so quick that it is hard to hit, or so
slow that two walks run — send a word on the pace either way. Two known edges are not findings: the
run shows the walk clip; and a strike is chosen when its key is pressed, so a `run_kick` that waits
in the buffer stays `run_kick` and rolls even from a stand once it starts, the same edge as the
`jump_kick` of §22 step 5. Neon Rumble cannot show it: an L on the run starts the kick in that tick,
a busy fighter is no longer running, and an L pressed right after a hit lands still reads the run,
waits behind the hurt and expires there — the buffer of 8 ticks is shorter than the shortest hitstun
of 10. The edge is
pinned by `framework_brawl_run_test` (a slide row buffered behind a jab starts without a run and
rolls) rather than seen in the window.

Send back: the terminal lines of steps 4–5 and one word on the pace of the double tap.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 26. Gate 6 of #25 — the block and the roll

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки react tick 101 banderas block, react tick 111 banderas stand, react tick 353 banderas dodge; окно neon_rumble: I держит позу блока на месте, отпускание выходит; O — перекат вперёд около 0,4 с сквозь adler, J/K/I/O во время переката ничего не делают; I и O в прыжке и в ударе не срабатывают; прислать строки react, слово о длине переката и частоту экрана -->

B3e of spec #25 gave the fighters a block and a roll. I held on the ground is the block: the fighter
takes the `block` pose, stands where he is while it is held and leaves it on the release, with no
recovery. A light or heavy strike from the front into the block deals nothing: both fighters stop for
the strike's hitstop and the blocker slides back at half the strike's push. A strike from behind, a
launch and (from B3f) a grab go through as usual, and the attacker's chain goes on after a blocked
strike as after a hit. O on the ground is the roll: the fighter rolls forward at `run_x` for `dodge`
ticks (Rainbird 20, Banderas 24, Adler 28 — a third to half a second at 60 Hz), cannot be hit on any
of them, and nothing stops or restarts it. The simulation steps once per drawn frame, so on a 120 Hz
screen the roll is half as long — not a finding. The roll may start from the block. Neither starts in the air, in a
strike or in a hurt.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS` and, in each run, among the lines of §25 step 1,

       neon-rumble: react tick 101: banderas block
       neon-rumble: react tick 111: banderas stand
       neon-rumble: react tick 353: banderas dodge

   The script holds I for ticks 100–109 and presses O on tick 352, right after the run kick of §25
   ends; Banderas rolls on past the fallen Adler and is still rolling at tick 360.

2. Start the window from a terminal you can read:

       ./build-sdk-work/game-Release/neon_rumble

3. **The block.** Hold I. Banderas takes the block pose and the terminal prints
   `neon-rumble: react tick <n>: banderas block`. Hold a direction as well: he does not walk. Let go of
   I — `banderas stand`, and the held direction walks him at once. Hold I in a jump or in the middle of
   a jab: nothing happens until he lands or the jab ends, and then the block starts if I is still held.
   Press J while blocking and let go of I within 8 ticks (0.13 s at 60 Hz): the jab comes out after
   the release. A J held in the block longer than that expires in the buffer — not a finding.
4. **The roll.** Press O. Banderas rolls forward, the way he faces, at the speed of the run (1.5× the
   walk), for 24 ticks — 0.4 s at 60 Hz, and the terminal prints `banderas dodge` and then
   `banderas stand`. Press J, K, I or O during the roll: it goes on to its end unchanged. Roll at
   Adler on his line: Banderas passes through him. O in a jump or in a strike does nothing; O while
   holding I rolls out of the block.

What counts as a finding: a block in the air or in a strike; a block that walks or does not end on the
release; a roll that goes backwards, stops early, turns or starts again on a second O; a roll in the
air; a pose that is not the block or the roll; a roll that feels too long or too short to use — send a
word on its length either way.

Four known edges are not findings. Nobody strikes Banderas in Neon Rumble until the AI of B5, so the
blocked strike (no damage, the shared stop, the half push) and the roll through a strike are not
visible in the window — they are pinned by `framework_brawl_guard_test`, `framework_brawl_dodge_test`
and the `block-turn` and `dodge-through` scenarios of the crossplay gate. Fighters do not push each
other (the `push` box of B3f is what a grab takes, not a wall), so the roll passing through Adler says nothing about invulnerability. Only Banderas
is under the keyboard, so the rolls of Rainbird (20 ticks) and Adler (28) wait for the AI of B5. The
`dodge` clip is 7 frames of 60 ms, 4 ticks each, 28 in all, and the roll of Banderas is 24: its last
frame is never shown and he snaps to the stand pose (Rainbird's 20 would lose two). The lengths are the
owner's choice of 2026-10-09; say so if the cut reads wrong.

Send back: the terminal lines of steps 3–4, one word on the length of the roll and the refresh rate of
the screen it was judged on.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## 27. Gate 7 of #25 — the grab, the throw and the hit on the ground

<!-- gate: open | bash scripts/check_sdk_game.sh --keep — PASS и строки hit tick 198 banderas/jump_kick -> adler, hit tick 417 banderas/grab -> adler, react tick 417 adler thrown, react tick 436 adler down; окно neon_rumble: H у стоящего adler — захват и бросок вперёд, adler летит от места banderas, ложится и встаёт, banderas доигрывает throw; H в прыжке, по лежащему и встающему ничего не берёт; J в прыжке по лежащему adler отнимает hp без реакции, jab и cross по лежащему — нет; прислать строки hit и react и слово о дальности броска -->

B3f of spec #25 gave the fighters a grab, a throw and a hit on the ground. H on the ground is the grab:
the fighter plays `reach`, and its active frames take a target that stands, walks, is hurt or blocks on
the ground — through the block. The taken target loses the grab's `damage` (6) at once and flies
`thrown` from the spot of the thrower, forward the way the thrower faces, at the throw speed of the
table (3 across, 4 up); it cannot be hit in the flight, strikes the enemies of the thrower it flies
into, lands lying (`down`) and rises (`getup`). The thrower plays `throw` to its end and does nothing
else meanwhile. A target in the air, falling, lying, rising or rolling cannot be taken; H in the air
does nothing, and H in a strike waits for its end like any strike key. A strike with `hits_down` hits a
fighter lying on the ground: damage and the hitstop, no reaction and no push. In Neon Rumble only
`jump_kick` (J in a jump) has it. Any strike that is not blocked and lands on a fighter in the air makes
him fall.

Run from the repository root (Windows: from the `scripts\win-dev.bat shell` window, with `.exe` and
backslashes).

1. Build and check:

       bash scripts/check_sdk_game.sh --keep

   Expected: `sdk-game: PASS` and, in each run, among the lines of §25 step 1,

       neon-rumble: hit tick 198: banderas/jump_kick -> adler, damage 10, hp 60
       neon-rumble: hit tick 417: banderas/grab -> adler, damage 6, hp 42
       neon-rumble: react tick 417: adler thrown
       neon-rumble: react tick 436: adler down
       neon-rumble: react tick 466: adler getup

   The script jumps on tick 180 and presses J on 181: the jump kick lands on Adler, lying since tick
   174, with no reaction line. It presses H on tick 408 next to the standing Adler.

2. Start the window from a terminal you can read:

       ./build-sdk-work/game-Release/neon_rumble

3. **The grab.** Walk Banderas up to Adler on his line and press H. Adler flies forward, away from
   Banderas, and the terminal prints `hit tick <n>: banderas/grab -> adler, damage 6`, `adler thrown`,
   then `adler down`, `adler getup` and `adler stand`. Banderas stands in the `throw` pose until it
   ends and no key cuts it short; a key pressed in its last 8 ticks waits for its end, as in §24.
   Press H while Adler falls, lies or rises, and H in a jump: nothing is taken.
4. **The hit on the ground.** Knock Adler down (L on the run), jump next to him (K or Space) and press
   J in the air: the jump kick hits him lying — `hit tick <n>: banderas/jump_kick -> adler`, no
   `react` line, and his hp goes down. A jab or a cross at the lying Adler makes no line.

What counts as a finding: a grab in the air or of a target that falls, lies or rises; a thrown Adler who
flies backwards, starts away from Banderas, is hit in the flight or does not lie down and rise after
landing; Banderas walking, striking, blocking or rolling before the throw ends; a hit on the lying Adler
by anything but the jump kick, or one that makes him react or slide; a pose that is not `reach`, `throw`
or `thrown`; a throw that reads too short or too long — send a word on its distance either way.

Known edges, not findings. Nobody but Banderas moves until the AI of B5, so the thrown body striking
another enemy, a grab against a strike in the same tick, two grabs of one target (both are torn) and a
grab of a grabber are not visible in the window, nor is a grab through a block or of a hurt target:
Adler never blocks, and the 10 ticks of hurt after a jab end before a `reach` pressed in the jab lands —
they are pinned by `framework_brawl_grab_test`, `framework_brawl_clash_test`,
`framework_brawl_throw_test` and the `grab-throw-stomp` and `grab-clash` scenarios of the crossplay
gate. The `throw` clip is 10 frames of 70 ms, about 42 ticks, while the thrown Adler flies about 19 and
lies 30: Banderas has a few ticks to finish his own victim. The hitstop of a hit on the ground holds the
lying timer as it holds everything else, so the getup comes later by that much (204 → 209 in the
script). One grab may take two targets standing in its reach in one tick. A grab torn in one tick may
take on the next active frame of the same `reach`. The thrown Adler faces the way Banderas faces, and
frame 101 of `thrown` draws him 5–7 px below his pivot. The length of the roll against its 28-tick clip
is the open question of §26, not of this gate.

Send back: the terminal lines of steps 3–4 and one word on the distance of the throw.

When the window closes, the last line is `neon-rumble: window run ok, <n> frames`, and the exit code
is 0.

## Beyond the gates

The gates above are what the ADRs waited on; 14 of the 28 are closed, and the open 14 are listed by
`scripts/owner_check.sh`, which reads the marks under the headings above rather than repeating them.
Of the two of spec #22, §14 lost its blocker on 2026-09-04 and now waits only for a second machine.
A machine with a screen, speakers and a pad can
also exercise things no gate covers — playing the sample game long enough to hear the audio, the
achievement toast surviving a restart, the offscreen `--demo` render path, an output device yanked
mid-frame, and `assetc` reproducing `bundle_hash = 0x1a557ae839e76ea0` byte for byte on another OS.
Those scenarios, with the exact commands per platform, are sections A–F of
[`owner-setup.txt`](owner-setup.txt). Section R of the same file is the mobile pair: both shells
built from the root by `scripts/xcompile_verify.sh`, the sample game started in the iOS simulator
with its five startup lines, and what to look for on an iPhone and an Android phone — the stick and
the fire button held at once, and sound. The simulator half has been run; the two phones have not,
and only you have them.

Two gates that CI covers on Linux only live in the same file, as section Q: `plugin-wasm` (the escape
gate and the `native == WASM` golden) and `plugin-wasm-host` (what the host object says about
itself). They need the wasmtime C-API, which is not in git: CI fetches it by its pin and runs both
under ASan/UBSan, while on macOS nothing but your machine executes them. A machine with `deps/`
unpacked runs both in a second, and section Q carries the commands with their output line by line.
Re-run them when a commit touches `engine/plugin/wasm_*`.

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
  for the Windows package that came back from CI.
- For §16, the answer to `where vcruntime140.dll` on the clean box (it decides whether the rest of
  the section counts), the `dir like-nes\bin` listing, `%ERRORLEVEL%` from the `/qn` install, and
  the `dir` after the `/qn` uninstall. A screenshot if any dialog appeared at all — under `/qn`
  there should be none.
- For §17, the list of everything you had to add that the page did not say — package names,
  flags, the shell you had to open — and the point where you stopped. An empty list is the result
  the gate is hoping for and the one that needs saying out loud; "it worked" without the list is
  indistinguishable from not having run it.
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
