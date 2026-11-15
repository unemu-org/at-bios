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
nix build                    # same as .#unemu-at
nix build .#unemu-at         # result/bios-unemu-at.bin: the MACHINE_UNEMU_AT config
nix build .#xi8088           # result/bios-xi8088.bin: upstream Xi 8088 config, unchanged
nix build .#xi8088-atreset   # Xi 8088 plus FLOPPY_RESET_ANY_DRIVE: the image
                             # unemupc98's open AT profile boots until it moves
                             # to unemu-at
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
| `unemu-at` | `db6eb04faabe58378246f7c5bf14ec2a316ca8bdb80839b137a8ec850c1dcf64` |

## Deviations from upstream

All of them are behind unemu defines; upstream's configurations assemble to
the same bytes as before (`xi8088` is pinned).

- `FLOPPY_RESET_ANY_DRIVE` (`src/config.inc`, used in `src/floppy2.inc`):
  INT 13h AH=00h (reset) accepts any DL, including fixed-disk reset requests,
  as the IBM PC AT BIOS does. Upstream rejects a reset for a drive number
  above 1 or for a drive that is not configured. The `xi8088-atreset` variant
  is Xi 8088 plus this define; it gives the same bytes as the former
  `patches/at-reset.patch` build.
- `MACHINE_UNEMU_AT` (`src/config.inc`): the configuration of the unemu AT
  board, an emulated IBM PC/AT-compatible 80286 board. It starts from
  `MACHINE_XI8088` and keeps its settings (model byte FCh, AT RTC and CMOS
  setup, NMI mask through port 70h with the Xi 8088 polarity, two 8259s, 8042
  keyboard controller, PS/2 aux port with a 1 KiB EBDA, port 61h refresh-bit
  delays). The differences:
  - no `TURBO_MODE`: the Xi 8088 turbo switch is bit 2 of port 61h, which on
    the AT disables the RAM parity check. The turbo setup option, the CPU
    clock line and the Ctrl+Alt+Gray-/+ hot keys are left out;
  - POST port 61h initialisation (`src/bios.asm`): 0Ch clears and disables
    both ~IOCHK and the RAM parity check (on the AT the parity latch clears
    only through bit 2), then 00h enables both with the speaker off; the
    Xi 8088 writes only 08h before 00h, and its 00h also means turbo off;
  - `FLOPPY_RESET_ANY_DRIVE` is set (above);
  - `AT_DISPLAY_SWITCH` (`src/config.inc`, used in `src/at_kbc.inc`): the
    display type switch, bit 6 of the 8042 input port (command C0h), is read
    with the IBM PC/AT polarity: 0 (jumper installed) selects the colour
    adapter, 1 (no jumper) the monochrome adapter, and POST sets the
    equipment word's video bits (4–5) to 80x25 colour or monochrome
    accordingly. The Xi 8088 switch SW2-8 reads the other way (off, bit 6
    set, is CGA), so the unemu machines can use the same switch setting
    for this BIOS and the IBM one;
  - banner (`src/messages.inc`): "unemu AT BIOS, Version 1.0.2-unemu.1",
    Sergey Kiselev's copyright line unchanged, and a line crediting the
    unemu contributors. `VERSION` is overridden for this configuration only;
    the ROM date stays upstream's.

## Plan

The `MACHINE_UNEMU_AT` configuration targets an IBM PC/AT-compatible board
(80286, two 8259s, two 8237s, 8042, MC146818). It still has to gain at least
what AT software expects of the system BIOS:

- the CMOS shutdown-byte resume after a processor reset (codes 05h, 09h and
  0Ah through 0040:0067h);
- INT 15h AH=87h (block move) and AH=88h (extended memory size);
- A20 control through the 8042;
- CMOS configuration checks, with the checksum over 10h–2Dh (the inherited
  NVRAM setup sums only 10h–20h and keeps its own configuration byte at 2Dh).

Inherited behaviour that is not yet AT-correct:

- the NMI mask polarity of port 70h is the reverse of the IBM AT's (bit 7
  set masks NMI on the AT);
- CPU detection knows only the 8088, 80C88 and V20, so an 80286 is reported
  as an 8088;
- only the first 8237 is initialised (the second, with channel 4 in cascade
  mode, is not), and port C0h, the second 8237's channel 4 address register,
  is used as a scratch I/O-delay port.

The work is tracked in this repository's beads (prefix `uab`). unemupc98 fetches
released images by SHA-256 and lists them in its firmware catalogue as open
firmware.
