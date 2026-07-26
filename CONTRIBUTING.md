# Contributing to like-nes

Contributions are welcome. This document covers the two things that are not
negotiable — the sign-off and the branch flow — and points at the rest.

## Developer Certificate of Origin

like-nes uses the [Developer Certificate of Origin](https://developercertificate.org/)
(DCO) rather than a CLA. You keep the copyright to your work; you are not
transferring anything to anyone. You are certifying that you have the right to
submit it, and that it may be distributed under the project's license.

Sign off every commit:

```
git commit -s -m "feat(render): add sprite batching"
```

That appends a trailer to the commit message:

```
Signed-off-by: Your Name <your.email@example.com>
```

The name and email must be real and must match the commit author or the
committer (a maintainer who rebases your branch signs as committer). Comparison
is case-insensitive. CI rejects a pull request whose commits are not all signed
off.

Forgot to sign off? Amend the last commit with `git commit -s --amend --no-edit`,
or fix a whole branch with `git rebase --signoff <base branch of your PR>`
(usually `dev`), then force-push.

History predating the DCO gate is exempt: `.github/workflows/dco.yml` skips
every commit reachable from the `DCO_SINCE` pin. Only work merged after the gate
landed has to be signed.

By signing off you agree that your contribution is dual-licensed **MIT OR
Apache-2.0**, matching the project (see [`LICENSE`](LICENSE)). No additional
terms apply, and no separate agreement will be requested of you.

**Why DCO and not a CLA.** A CLA would let the project relicense your work
unilaterally — including under non-free terms. like-nes has no commercial arm
and no plan for one, so there is nothing a CLA would buy that would not also
buy the ability to take the project closed. The trade-off is that the license
is effectively locked: changing it later would require every contributor's
consent. That is intentional.

## Branches and pull requests

| Branch | Role |
|---|---|
| `main` | Release branch, reflects the confirmed state of the engine. No direct commits — merge from PR only. |
| `dev` | Working branch: code, PoC, hypothesis checks. |

Feature branches (`feat/*`, `fix/*`, `poc/*`) branch from `dev` and merge back
into `dev`. Each completed round opens a PR from `dev` into `main`. Details are
in [`CLAUDE.md`](CLAUDE.md).

## Before you open a PR

- CI must be green on all three OSes (`.github/workflows/ci.yml`).
- Commits follow conventional commits, in English: `feat(scope): …`,
  `fix(ci): …`, `docs: …`.
- Do not commit code that fails the linter, type checks, or the build.
- Adding or bumping a dependency? It must be permissively licensed and
  compatible with MIT **and** Apache-2.0, and you must update **both**
  [`THIRD-PARTY.md`](THIRD-PARTY.md) (inventory) and the notice files
  ([`THIRD-PARTY-NOTICES.txt`](THIRD-PARTY-NOTICES.txt) and
  [`THIRD-PARTY-NOTICES-NDK.txt`](THIRD-PARTY-NOTICES-NDK.txt) by hand,
  [`THIRD-PARTY-NOTICES-RUST.txt`](THIRD-PARTY-NOTICES-RUST.txt) by running
  `scripts/gen_rust_notices.py`) in the same PR. Copyleft dependencies
  (GPL, LGPL, AGPL) and source-available licenses (BSL, SSPL, Elastic) will not
  be merged — they would impose terms on every game built with the engine.

## Project context

Design specs, architecture decision records, and conventions live in
[`.context/`](.context/). Read the relevant spec before changing a subsystem;
each one records why the current design is what it is.
