# AGENTS.md — unemu-at-bios (unemu fork of 8088_bios)

Repo-specific notes for coding agents. Generic software-engineering advice is out of scope.

## What this is

A fork of **Sergey Kiselev's 8088_bios** (GPL-3.0-or-later), written for the Xi 8088
and Micro 8088 boards, adapted into the open **AT BIOS** of the unemu family of
emulators. Its first consumer is **unemupc98**, an IBM PC/AT-class emulator whose
open-firmware AT profile needs a system BIOS that behaves like the IBM PC/AT BIOS
where AT software depends on it (CMOS shutdown-byte resume, INT 15h extended memory
services, A20 through the 8042, CMOS checks), so public machine images never depend
on a proprietary BIOS. See `README.unemu.md` for the build, the pinned variants,
the deviations and the plan.

## Language

- Talk to the maintainer in Polish.
- Everything committed is English: code, comments, docs, commit messages, bead text.

## Work tracking

Open work lives in **beads** (`bd`, prefix `uab`): `bd ready`, `bd show <id>`.
The shared working rules are beads memories; read them at session start with
`bd memories` (`bd recall <key>` for one): roles, fork-rules, target-machine,
build-and-release, publication-rule, bead-workflow. Cross-repository ids carry a
tag, e.g. `upc-fro.11 (unemupc98)`; an untagged id is ours.

## Hard rules

- **Never commit without the maintainer's request** (may be granted for a session).
  `bd init` and some bd commands commit on their own: check `git log -1` after them.
- **The original author stays visible**: the upstream history is kept in full and
  unchanged; our changes are minimal diffs on top, each deviation listed in
  `README.unemu.md`. Keep the original copyright and licence notices; add an unemu
  contributor line only where a file changes substantially. No reformatting of
  upstream code.
- **Upstream configurations stay byte-identical**: unemu changes go behind the
  `MACHINE_UNEMU_AT` configuration (or another unemu define), so `nix build .#xi8088`
  keeps upstream's bytes; `nix flake check` pins them.
- **Reproducible**: `nix build` must give the same ROM bytes for the same source;
  every variant's SHA-256 is pinned in `flake.nix`.
- **Publication rule**: write as if public; no private hosts, paths or personal
  setup; never create or publish a repository, which is the maintainer's step. No
  proprietary ROM bytes in this repository, ever. Reading the IBM PC AT Technical
  Reference BIOS listing to match behaviour is fine; copying its code is not.

## Quick loop

```sh
nix build .#xi8088-atreset    # result/bios-xi8088-atreset.bin, .lst, .bin.sha256
nix flake check               # every variant rebuilt and checked against its pin
nix develop                   # nasm, patch, make, cmake
```

The emulator-side qualification (POST, FreeDOS boot, the AT protected-mode test
floppy) runs in unemupc98; the supervisor runs it when ROM behaviour changes.

<!-- BEGIN BEADS INTEGRATION v:1 profile:minimal hash:1105d646 -->
## Beads Issue Tracker

This project uses **bd (beads)** for issue tracking. Run `bd prime` to see full workflow context and commands.

### Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work
bd close <id>         # Complete work
```

### Rules

- Use `bd` for ALL task tracking — do NOT use TodoWrite, TaskCreate, or markdown TODO lists
- Run `bd prime` for detailed command reference and session close protocol
- Use `bd remember` for persistent knowledge — do NOT use MEMORY.md files

**Architecture in one line:** issues live in a local Dolt DB; sync uses `refs/dolt/data` on your git remote; `.beads/issues.jsonl` is a passive export. See https://github.com/gastownhall/beads/blob/main/docs/core-concepts/sync-concepts.md for details and anti-patterns.

## Agent Context Profiles

The managed Beads block is task-tracking guidance, not permission to override repository, user, or orchestrator instructions.

- **Conservative (default)**: Use `bd` for task tracking. Do not run git commits, git pushes, or Dolt remote sync unless explicitly asked. At handoff, report changed files, validation, and suggested next commands.
- **Minimal**: Keep tool instruction files as pointers to `bd prime`; use the same conservative git policy unless active instructions say otherwise.
- **Team-maintainer**: Only when the repository explicitly opts in, agents may close beads, run quality gates, commit, and push as part of session close. A current "do not commit" or "do not push" instruction still wins.

## Session Completion

This protocol applies when ending a Beads implementation workflow. It is subordinate to explicit user, repository, and orchestrator instructions.

1. **File issues for remaining work** - Create beads for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **Handle git/sync by active profile**:
   ```bash
   # Conservative/minimal/default: report status and proposed commands; wait for approval.
   git status

   # Team-maintainer opt-in only, unless current instructions forbid it:
   git pull --rebase
   git push
   git status
   ```
5. **Hand off** - Summarize changes, validation, issue status, and any blocked sync/commit/push step

**Critical rules:**
- Explicit user or orchestrator instructions override this Beads block.
- Do not commit or push without clear authority from the active profile or the current user request.
- If a required sync or push is blocked, stop and report the exact command and error.
<!-- END BEADS INTEGRATION -->

<!-- BEGIN BEADS CODEX SETUP: generated by bd setup codex -->
## Beads Issue Tracker

Use Beads (`bd`) for durable task tracking in repositories that include it. Use the `beads` skill at `.agents/skills/beads/SKILL.md` (project install) or `~/.agents/skills/beads/SKILL.md` (global install) for Beads workflow guidance, then use the `bd` CLI for issue operations.

### Quick Reference

```bash
bd ready                # Find available work
bd show <id>            # View issue details
bd update <id> --claim  # Claim work
bd close <id>           # Complete work
bd prime                # Refresh Beads context
```

### Rules

- Use `bd` for all task tracking; do not create markdown TODO lists.
- Run `bd prime` when Beads context is missing or stale. Codex 0.129.0+ can load Beads context automatically through native hooks; use `/hooks` to inspect or toggle them.
- Keep persistent project memory in Beads via `bd remember`; do not create ad hoc memory files.

**Architecture in one line:** issues live in a local Dolt DB; sync uses `refs/dolt/data` on your git remote; `.beads/issues.jsonl` is a passive export. See https://github.com/gastownhall/beads/blob/main/docs/core-concepts/sync-concepts.md for details and anti-patterns.
<!-- END BEADS CODEX SETUP -->
