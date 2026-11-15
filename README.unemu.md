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
| `unemu-at` | `2534ed978ba5f76aabda2579800ee9f418f00b983f9e9b3edbe6a52805872868` |

## Deviations from upstream

All of them are behind unemu defines; upstream's configurations assemble to
the same bytes as before (`xi8088` is pinned).

- `FLOPPY_RESET_ANY_DRIVE` (`src/config.inc`, used in `src/floppy2.inc`):
  INT 13h AH=00h (reset) accepts any DL, including fixed-disk reset requests,
  as the IBM PC AT BIOS does. Upstream rejects a reset for a drive number
  above 1 or for a drive that is not configured. The `xi8088-atreset` variant
  is Xi 8088 plus this define; it gives the same bytes as the former
  `patches/at-reset.patch` build.
- `AT_SHUTDOWN` (`src/config.inc`; the code is in `src/at_shutdown.inc`,
  included from `src/bios.asm`): the soft-reset handling of the IBM PC AT
  BIOS. The reset vector and `cold_start` stay at F000:FFF0h and
  F000:E05Bh; `cold_start` now first jumps to `shutdown_check`, before
  anything writes RAM, programs the PICs or DMA or sends an 8042 command,
  and without a stack. The entry paths:
  - power-on (8042 system flag, status bit 2, clear; POST sets it at the
    end of `kbc_init`): CMOS 0Fh (the shutdown byte) is cleared, so a stale
    code cannot take over the next soft reset, then a cold POST, which
    clears the warm boot flag at 0040:0072h;
  - soft reset (system flag set: a CPU reset through the 8042, a triple
    fault, or a jump to the reset vector after POST): CMOS 0Fh is read with
    NMI masked (8Fh to port 70h, AT polarity); except for codes 09h and
    0Ah, the coprocessor is reset (port F1h) and both 8259s are
    reinitialised, all masked; 0Fh is cleared to 00h (another reset then
    runs the POST), SS:SP is set to 0000:0400h, and by the code:
    - 05h: reads port 60h, sends EOI to the master 8259 and far-jumps
      through 0040:0067h (both masks read FFh);
    - 0Ah: far-jumps through 0040:0067h, the PIC masks left as they were;
    - 09h (return from INT 15h AH=87h): a cold start until the block move
      exists (`shutdown_block_move`);
    - 04h: POST, which then boots the OS through INT 19h. The IBM AT goes
      to INT 19h directly, but this BIOS's floppy code waits for IRQ6,
      which the masked PICs would never deliver;
    - 00h and any other code: POST, keeping 0040:0072h;
  - IOCHK NMI reboot (`int_02`, key 'r'): a forced cold POST through
    `shutdown_cold`, with 0Fh and 0040:0072h cleared and no dispatch;
  - Ctrl+Alt+Del: as upstream, straight to `warm_start` with 1234h.

  The warm boot flag is one-shot, as on the IBM AT: POST clears it before
  it boots the OS (`boot_os`), so 1234h set by Ctrl+Alt+Del or by a program
  before a soft reset skips the memory test once. A program resumed
  through 0040:0067h gets control in real mode with interrupts disabled and
  NMI masked; nothing on that path writes RAM or sends an 8042 command, so
  A20 stays as it was. Upstream clears 0040:0072h on every entry to
  `cold_start`, keeps it after POST, and has no shutdown-byte dispatch.
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
  - `FLOPPY_RESET_ANY_DRIVE` and `AT_SHUTDOWN` are set (above);
  - `AT_DISPLAY_SWITCH` (`src/config.inc`, used in `src/at_kbc.inc`): the
    display type switch, bit 6 of the 8042 input port (command C0h), is read
    with the IBM PC/AT polarity: 0 (jumper installed) selects the colour
    adapter, 1 (no jumper) the monochrome adapter, and POST sets the
    equipment word's video bits (4–5) to 80x25 colour or monochrome
    accordingly. The Xi 8088 switch SW2-8 reads the other way (off, bit 6
    set, is CGA), so the unemu machines can use the same switch setting
    for this BIOS and the IBM one;
  - `AT_CPU` (`src/config.inc`, used in `src/cpu.inc` and
    `src/messages.inc`): POST tells the 80286 apart before the 8088 and V20
    tests and prints "Intel 80286" (or "Intel 80386 or later") without the
    8088 interrupt warning. FLAGS bit 15 always reads 1 on the 8088, 80C88
    and V20 and 0 on the 80286 and later; in real mode the 80286 also keeps
    bits 14–12 (IOPL, NT) clear when POPF tries to set them, the 80386 does
    not. The test uses 8086 instructions only. The co-processor test accepts
    the control word 037Fh (80287XL, 80387) after FNINIT as well as 03FFh
    (8087, 80287), where upstream accepts only 03FFh; a co-processor is
    reported as "Intel 80287", as an 8087 cannot be used with an 80286 (an
    80387 with an 80386 or later is shown the same way);
  - `AT_DMA` (`src/config.inc`; the code is in `src/at_dma.inc`, reached by
    a jump from the POST DMA initialization in `src/bios.asm`, where there
    is no stack yet): after the first 8237, POST initialises the second
    (channels 4–7, ports C0h–DFh) as the IBM PC/AT POST does: a master
    clear (DAh), channel 4 in cascade mode (C0h to the mode register, D6h)
    and unmasked (00h to the single mask register, D4h), so that channels
    0–3 reach the bus through it. As upstream does for the first 8237, the
    other channels (5–7) get single mode, verify, and stay masked; the
    command register (D0h) and their page registers (8Bh, 89h, 8Ah) are
    cleared. The scratch port `unused_reg`, written to set the ISA data
    bus before the serial and parallel port detection reads and as an I/O
    delay in INT 17h, moves from C0h (on the AT, the second 8237's channel
    4 address register: every write there toggles that controller's byte
    flip-flop) to 8Eh, a DMA page register the AT does not use. Port 80h
    is not used for it, so the POST codes there stay those of the POST
    stages;
  - banner (`src/messages.inc`): "unemu AT BIOS, Version 1.0.2-unemu.1",
    Sergey Kiselev's copyright line unchanged, and a line crediting the
    unemu contributors. `VERSION` is overridden for this configuration only;
    the ROM date stays upstream's.

## Plan

The `MACHINE_UNEMU_AT` configuration targets an IBM PC/AT-compatible board
(80286, two 8259s, two 8237s, 8042, MC146818). It has the CMOS shutdown-byte
resume after a processor reset (`AT_SHUTDOWN`: codes 05h and 0Ah, and 04h
through the POST). It still has to gain at least what AT software expects
of the system BIOS:

- INT 15h AH=87h (block move) with its return through shutdown code 09h
  (dispatched, a cold start until then), and AH=88h (extended memory size);
- A20 control through the 8042;
- CMOS configuration checks, with the checksum over 10h–2Dh (the inherited
  NVRAM setup sums only 10h–20h and keeps its own configuration byte at 2Dh).

Inherited behaviour that is not yet AT-correct:

- the NMI mask polarity of port 70h is the reverse of the IBM AT's (bit 7
  set masks NMI on the AT); only the `AT_SHUTDOWN` code uses the AT's.

The work is tracked in this repository's beads (prefix `uab`). unemupc98 fetches
released images by SHA-256 and lists them in its firmware catalogue as open
firmware.
