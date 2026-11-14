# unemu-at-bios (unemu fork of 8088_bios)

A fork of **Sergey Kiselev's [8088_bios](https://github.com/skiselev/8088_bios)**
for the machines of the unemu family, starting with unemupc98 (an IBM
PC/AT-class emulator). The upstream BIOS is written for the Xi 8088 and Micro
8088 boards. This fork adapts it, with small and reviewable changes, into the
open **AT BIOS** of unemu's 80286 machine profiles, so that public machine
images never depend on a proprietary system BIOS.

- **Author of the BIOS:** Sergey Kiselev. The upstream history is kept in full;
  every commit before the first unemu commit is his work, unchanged.
- **This fork:** the unemu contributors, only as additions for the unemu
  machines. Files we change substantially gain a contributor line next to the
  original copyright; the original notices stay as they are.
- **Licence:** GNU GPL version 3 or (at your option) any later version, as
  upstream (`LICENSE.txt`).
- **Upstream base:** `ce2ac99` on upstream `master` (BIOS version 1.0.2); the
  remote is named `upstream`.

## Build

```sh
nix build .#xi8088           # result/bios-xi8088.bin: upstream Xi 8088 config, unchanged
nix build .#xi8088-atreset   # the image unemupc98's open AT profile boots today
nix flake check              # rebuilds every variant and checks its pinned SHA-256
nix develop                  # nasm, patch, make, cmake
```

Each variant is the raw 32 KiB image that maps at F000:8000 (physical
F8000h–FFFFFh); the CPU reset entry at F000:FFF0 is at offset 7FF0h. The
128 KiB flash-programmer images of the upstream build are not produced. The
nixpkgs revision is pinned to the one unemupc98 uses, because the ROM bytes
depend on the nasm version.

| Variant | SHA-256 |
|---|---|
| `xi8088` | `1ad458acb1f22dee6b3d9b6bd95e83218a7ddc5389fa2b2c67a4001e689c81a8` |
| `xi8088-atreset` | `61589573c9809273198b137d9419c87242b0a5f63c2ad834a9565950c3d6becc` |

## Deviations from upstream

- `patches/at-reset.patch`, applied only by the `xi8088-atreset` variant:
  INT 13h AH=00h (reset) accepts any DL, including fixed-disk reset requests,
  as the IBM PC AT BIOS does. It is the first change to move into the source
  behind an unemu AT configuration.

## Plan

The goal is a `MACHINE_UNEMU_AT` configuration for an IBM PC/AT-compatible
board (80286, two 8259s, two 8237s, 8042, MC146818). It must at least provide
what AT software expects of the system BIOS:

- the CMOS shutdown-byte resume after a processor reset (codes 05h, 09h and
  0Ah through 0040:0067h);
- INT 15h AH=87h (block move) and AH=88h (extended memory size);
- A20 control through the 8042;
- the AT display switch read with IBM polarity;
- CMOS configuration checks.

The work is tracked in this repository's beads (prefix `uab`). unemupc98 fetches
released images by SHA-256 and lists them in its firmware catalogue as open
firmware.
