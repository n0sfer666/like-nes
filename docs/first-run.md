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

## Related

- [`owner-verification.md`](owner-verification.md) — the gates a CI runner cannot close (real GPU
  session, real gamepad) and `scripts/owner_check.sh`, which closes their automated half.
- `CONTRIBUTING.md` — branches, DCO sign-off, what to run before a PR.
- `.context/checks.md` — the checks CI and the pre-commit hook run.
- `.context/env.md` — environment variables the runtime and the gates understand.
