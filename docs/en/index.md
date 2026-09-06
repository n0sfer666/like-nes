# like-nes documentation

English · [Русский](../ru/index.md)

Public documentation for people **using** the engine. Design specs, architecture decision records
and working notes live in [`.context/`](../../.context/) — they are development material, and
nothing here assumes you have read them.

## Written

- **[Getting started](getting-started/prerequisites.md)** — what each OS needs, how to build, and
  the first run of the editor and the sample games.
  - [Prerequisites](getting-started/prerequisites.md)
  - [Build](getting-started/build.md)
  - [First run](getting-started/first-run.md)
- **Guide** — one subsystem per page, with every snippet taken from a program in
  [`docs/examples/`](../examples/) that CI builds and runs on three operating systems.
  - [The tick and the schedule](guide/tick-and-schedule.md)
  - [Determinism and fixed-point arithmetic](guide/determinism.md)
- **[Contributing](../../CONTRIBUTING.md)** — branches, DCO sign-off, what to run before a PR.
- **[License and third-party components](../../LICENSE)** — `MIT OR Apache-2.0`, and the inventory
  of what the engine links.

## Not written yet

The sections below are planned by spec #19 and are **not** in the tree. They are listed unlinked on
purpose: a link to a page that does not exist is worse than an honest gap, and this project's
documentation gate treats a dead link as a failure.

- **Tutorial** — "your first game", from an empty project to a playable character on a tilemap with
  animation, sound and an achievement.
- **Guide**, the rest of it: assets and baking, input and rebinding, physics, the character
  controller and tilemaps, sprites/animation/camera/particles, materials and shaders, audio,
  plugins, achievements, packaging and release.
- **Architecture** — the engine's invariants (determinism, layer boundaries, failure isolation) and
  why they are what they are, with links to the ADRs.
- **Reference** — API reference generated from the headers.
- **FAQ and troubleshooting** — the failures each OS produces, with their exact messages.

## How these two languages relate

English is the source; Russian is a translation of it. Every Russian page carries the checksum of
the English page it was translated from, and `bash scripts/check_docs.sh` reports a translation that
has fallen behind — as an error for the README and getting-started, as a warning elsewhere. The two
trees must mirror each other file for file.
