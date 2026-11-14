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
