# First run: where it went, and the numbers that stayed

The clone → build → editor instructions that used to live here are now **public documentation**, in
both languages (spec #19):

- [`docs/en/getting-started/prerequisites.md`](en/getting-started/prerequisites.md) — what each OS
  must provide, the Windows shell question, Wayland
  ([ru](ru/getting-started/prerequisites.md))
- [`docs/en/getting-started/build.md`](en/getting-started/build.md) — clone, configure, options,
  the build gate ([ru](ru/getting-started/build.md))
- [`docs/en/getting-started/first-run.md`](en/getting-started/first-run.md) — the editor, the
  sample games, hot-reload, and what a failure looks like ([ru](ru/getting-started/first-run.md))

This file is kept because other documents link to it, and because one thing here is **not** public
documentation: the measured cost of the edit → build → hot-reload loop, which spec #13 gate 8 wants
as a fact per OS. `scripts/owner_check.sh` prints it — `best` and `median` of three timed runs.

| OS | machine | best | median |
|---|---|---|---|
| Windows 11, MSVC 14.44 | i7-8550U, NVIDIA MX150 | 1.49 s | 1.59 s |

The Linux row is still empty. Run `owner_check.sh` there and fill it in — the point of the number is
the comparison between the two, and one cell answers nothing.

Until 2026-08-30 the Windows cell could not be filled at all, and the reason is worth keeping: the
stage handed the binary to Python as a *relative* path with forward slashes, which `CreateProcess`
does not read as a path at all, so the stage died on a traceback. That traceback went into a report
whose verdict still said `owner-check: PASS`, because the stage's exit status was swallowed by its
own `| tee` and never counted. A gate that prints a stack trace and passes is worse than one that is
missing.

## Your own game against the SDK

`games/neon-rumble` is built **against an installed SDK prefix**, not inside the tree (spec #24).
`bash scripts/check_sdk_game.sh --keep` builds and installs the `sdk` component in Release and Debug,
builds the game against that prefix and leaves it in `build-sdk-work/`. The game's `game.bundle` is
baked at build time by the SDK's `assetc` from `games/neon-rumble/game.manifest`, and its
`bundle_hash` must equal `games/neon-rumble/bundle.hash` on every OS. The game maps that bundle
and draws level 1 from it with Puffolotti fighters on their spawns: Banderas is player 1 and walks under WASD, W and S
move in depth, Space or K jumps, the camera follows him; Rainbird joins as player 2 when a pad
button, an arrow, numpad 0–6 or numpad − is pressed (§29 of `docs/owner-verification.md`), and the nearer fighter is drawn on top (F3 toggles the cel, pivot
and box overlay, F1 the credits screen in the monogram font); `--headless` prints the level, viewport policy, camera
bounds, frame, roster, depth band, brawl hash, font and credits summary the gate checks. The
steps for the owner's window run, with the expected output line by line, are section S of
[`owner-setup.txt`](owner-setup.txt).
The live check of the street — parallax, the signs, 21:9 and 4:3 windows, the credits screen and a
fresh save in Tiled — is §20 of [`owner-verification.md`](owner-verification.md).
How to write such a game yourself is the [guide](en/guide/sdk-and-your-game.md).

## Related

- [`owner-verification.md`](owner-verification.md) — the gates a CI runner cannot close (real GPU
  session, real gamepad) and `scripts/owner_check.sh`, which closes their automated half.
- `CONTRIBUTING.md` — branches, DCO sign-off, what to run before a PR.
- `.context/checks.md` — the checks CI and the pre-commit hook run.
- `.context/env.md` — environment variables the runtime and the gates understand.
