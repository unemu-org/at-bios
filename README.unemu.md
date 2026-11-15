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
| `unemu-at` | `e8a39cce7d0d7b7bdbede565342f124e2cf3e0c4daafc98e702d7cafc56fe079` |

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
    - 09h (return from INT 15h AH=87h or from the POST memory sizing):
      `block_move_return` with `AT_EXT_MEMORY` (below), a cold start
      without it;
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
  setup, NMI mask through port 70h, two 8259s, 8042 keyboard controller,
  PS/2 aux port with a 1 KiB EBDA, port 61h refresh-bit delays). The
  differences:
  - no `TURBO_MODE`: the Xi 8088 turbo switch is bit 2 of port 61h, which on
    the AT disables the RAM parity check. The turbo setup option, the CPU
    clock line and the Ctrl+Alt+Gray-/+ hot keys are left out;
  - POST port 61h initialisation (`src/bios.asm`): 0Ch clears and disables
    both ~IOCHK and the RAM parity check (on the AT the parity latch clears
    only through bit 2), then 00h enables both with the speaker off; the
    Xi 8088 writes only 08h before 00h, and its 00h also means turbo off;
  - `FLOPPY_RESET_ANY_DRIVE` and `AT_SHUTDOWN` are set (above);
  - `AT_EXT_MEMORY` (`src/config.inc`; the code is in `src/at_extmem.inc`,
    included from `src/bios.asm` and dispatched from `int_15` in
    `src/misc.inc`; requires `AT_SHUTDOWN` and `AT_CPU`): the extended
    memory services of the IBM PC/AT BIOS. Upstream returns AH=86h with CF
    set for both. The code uses 80286 instructions and is assembled only in
    this configuration.
    - INT 15h AH=88h returns in AX the extended memory size in KiB from
      CMOS 30h–31h. Later BIOSes clear CF; this one returns the caller's
      flags unchanged, as the IBM AT does. NMI is masked while CMOS is
      read, then port 70h is left at 0Dh (NMI enabled, AT polarity);
    - measured size in CMOS 30h–31h: on every POST, right after `rtc_init`,
      `ext_mem_size_init` measures the memory above 1 MiB, as the IBM POST
      does, stores the size in KiB in 30h–31h with NMI masked and returns it
      in AX. The test runs in 80286 protected mode through the block move's
      own entry and exit (`bm_run`, below, with the GDT on the POST stack and
      the sizing as the work step instead of the move), so each POST resets
      the CPU once more through the 8042, with shutdown code 09h. It counts 64
      KiB blocks from 100000h: the target descriptor is moved to each block in
      turn, AA55h is written at offset 0 and 55AAh at FFFEh, both are read
      back, and a block that keeps them is then cleared to zero, on every
      POST, as the IBM POST does, so nothing from the previous boot survives
      above 1 MiB (FreeDOS writes a VDISK header at 100000h, which would make
      its XMS driver refuse to load after a warm restart). The first block
      where either word does not read back ends the count, as does FE0000h,
      where the AT decodes its ROM, as on the IBM AT (at most EEh blocks,
      3B80h KiB); a size that is not a multiple of 64 KiB is rounded down. Two
      different words, read after both are written, fail on a bus that reads
      00h or FFh or the last value written; the 24-bit base does not wrap
      below 16 MiB. A parity or I/O channel check error latched by the test is
      cleared. Below 1 MiB only the POST stack and 0040:0067h–0040:006Ah (the
      frame pointer across the reset) change. An error (an exception in
      protected mode, an 8042 timeout) stores 0. A20 is disabled again on the
      way back, and the PICs keep the POST's setup (code 09h does not touch
      them). 30h–31h are outside every checksum range. As the IBM POST does,
      `AT_CMOS_CHECK` (below) then compares the measured size with the
      configured one, CMOS 17h–18h (164 on a mismatch);
    - the CPU reset: AH=87h and the POST sizing need the AT's CPU-only
      reset through the 8042 (FEh), which leaves the 8042 system flag,
      CMOS, RAM and the PICs as they were (IBM PC/AT, unemupc98). Where
      the 8042 reset is a full system reset instead (QEMU `isapc`
      clears the 8042 system flag and CMOS 0Fh, and keeps the other CMOS
      bytes and RAM, measured), the sizing reset comes back as a power
      on. So that such a machine still boots, `ext_mem_size_init` writes
      FFh to CMOS 31h before it sizes (no size reaches it, 15 MiB is
      3Ch); a POST that finds FFh there skips the sizing and reports the
      configured size, 17h–18h at most 3C00h, which also clears the mark
      (that POST does not clear the memory above 1 MiB).
      On such a machine each boot runs the POST twice (the sizing POST,
      cut off by the reset, then one with the configured size), and an
      AH=87h call reboots the machine instead of returning, as it also
      would with the IBM BIOS. A machine whose reset also clears the
      CMOS would still reset in a loop. A CMOS that reads FFh at
      31h at power on (QEMU seeds it so from 65 MiB up) gives one POST
      with the configured size;
    - INT 15h AH=87h moves CX words (up to 8000h) between the source and
      target descriptors of the caller's 48-byte GDT at ES:SI. The BIOS
      fills the GDT descriptor (08h, limit 2Fh), the code segment (20h,
      base F0000h) and the stack segment (28h, base SS×16); it saves the
      registers, DS and ES on the caller's stack and its SS:SP at
      0040:0069h/0040:0067h, disables interrupts, enables A20 through
      the 8042 (D1h, DFh), writes shutdown code 09h with NMI masked,
      enters protected mode (an IDT in ROM sends every exception vector,
      00h–1Fh, to a handler that records status 02h), moves the block,
      checks port 61h for a parity or I/O channel check error (status
      01h, latches cleared), and resets the CPU through the 8042 (FEh);
      if the CPU still runs about 30 ms later, it forces a triple fault
      (an interrupt with an empty IDT). The status is 02h from before
      protected mode until the move is done, so a triple fault, which
      also resumes through code 09h, returns an error.
      `block_move_return` restores SS:SP, disables A20 (D1h, DDh; status
      03h on a timeout unless an error is already recorded), writes 0Dh
      to port 70h (NMI enabled), restores the registers with AH = status
      and returns with RETF 2 and STI: CF = 0 and ZF = 1 for status 0,
      CF = 1 and ZF = 0 otherwise, the caller's flags dropped, as the IBM
      AT does. An A20 enable timeout returns status 03h the same way,
      without the reset. Everything but the move itself is `bm_run`, which
      runs the work step its caller names in DI in protected mode: the
      move for AH=87h, the memory sizing for POST, which calls it the way
      INT 15h is called (flags, CS, IP on the stack);
    - the status is kept across the reset in the saved AH on the
      caller's stack (the IBM AT keeps it in port 80h, which on the unemu
      AT board is the POST code port and reads FFh). The frame is the
      IBM one: the general registers, then ES, then DS;
    - A20 at POST: the 8042 may enable A20 at power on and on its
      self-test command (AAh, early in `kbc_init`, `src/at_kbc.inc`). As
      the IBM PC/AT POST does, right after the command byte that sets the
      system flag, `kbc_init` disables A20 with the block move's helper
      (D1h, DDh: output port bit 1 clear, bit 0 set, so no CPU reset),
      and the OS boots with A20 disabled. The write waits only for the
      8042 input buffer; a byte left in the output buffer stays there,
      as D1h returns none, and the auxiliary interface is disabled by
      then. A timeout halts with the 8042 command failure code, as for
      the command byte before it. Every POST does this: at power on,
      after a soft reset with a code other than 05h, 09h or 0Ah, after
      Ctrl+Alt+Del and after the IOCHK NMI reboot. The 05h, 09h and 0Ah
      resumes do not reach `kbc_init` (09h disables A20 in
      `block_move_return`). Upstream leaves A20 as the 8042 has it;
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
  - `AT_NMI_POLARITY` (`src/config.inc`, used in `src/bios.asm` and
    `src/rtc.inc`): port 70h bit 7 is the NMI mask with the IBM PC/AT
    polarity, set masks NMI. The inherited code is written for the Xi
    8088, where bit 7 set enables NMI (upstream ORs 80h into the register
    number to enable NMI and clears bit 7 to disable it). On the AT board
    it would leave NMI enabled during early POST, mask it with the POST
    "enable" and enable it again with every CMOS access. The NMI writes
    stay where upstream has them; each upstream line is kept for the
    other configurations, with the AT variant next to it, using the
    `nmi_disable_bit` and `nmi_enable_bit` equates:
    - POST masks NMI at `cpu_ok` (8Dh to port 70h) and enables it (0Dh)
      after the PIC and 8042 initialization, before the video and RTC
      initialization and the memory test, where upstream enables it (the
      IBM PC/AT POST enables it just before INT 19h);
    - the NMI handler (`int_02`) masks NMI on entry (8Dh). An NMI
      without the I/O channel check bit, such as the 80287 error that
      INT 75h passes on, returns with NMI enabled again (0Dh, through
      `rtc_read`), as the IBM PC/AT handler does; upstream returns with
      NMI disabled. After an I/O channel check, key 'i' enables NMI the
      same way, and 'd' leaves it masked until the next `rtc_read` or
      `rtc_write` with bit 7 clear (INT 1Ah, IRQ8) enables it;
    - `rtc_read` and `rtc_write` mask NMI during the access (the address
      ORed with 80h), then select register 0Dh with bit 7 of the caller's
      address as the NMI mask, as IBM's CMOS routines do: clear enables
      NMI, set leaves it masked. Every inherited caller passes bit 7
      clear and runs after the POST enable, so NMI is enabled after each
      CMOS access; the `AT_EXT_MEMORY` code passes it set and enables NMI
      when it is done. `AT_SHUTDOWN` and `AT_EXT_MEMORY` write port 70h
      directly with the same polarity;
  - `AT_CMOS_CHECK` (`src/config.inc`; the code is in `src/at_cmos.inc`,
    called from `rtc_init`, `get_floppy`, `get_config_a` and `nvram_save`
    in `src/rtc.inc` and from the POST and `boot_os` in `src/bios.asm`;
    requires `AT_RTC_NVRAM` and `AT_EXT_MEMORY`): the CMOS checks of the
    IBM PC/AT POST. The result goes to the diagnostic status byte, CMOS
    0Eh, with IBM's bits: 80h battery, 40h checksum, 20h configuration,
    10h memory size, 04h clock (08h, fixed disk, is never set):
    - `rtc_init`: RTC register D bit 7 (VRT) clear sets 80h, and 80h in
      0Eh stays set until the BIOS setup saves, as on the IBM AT (VRT
      reads 1 again after the first read). Otherwise the sum of 10h–2Dh
      must be non-zero and equal 2Eh–2Fh (high byte first), or 40h. The
      NVRAM checksum covers 10h–2Dh everywhere (`nvram_checksum`, used by
      `rtc_init` and `nvram_save`), where upstream sums 10h–20h; the
      configuration byte A at 2Dh is inside it. A CMOS saved by an
      upstream build therefore reads as a bad checksum (162) until the
      BIOS setup saves it;
    - with 80h or 40h the CMOS is not rewritten, as on the IBM AT. Upstream
      prints its own message, clears 10h–20h and writes its defaults;
      here `get_floppy`, `get_config_a` and the equipment byte read by
      `rtc_init` return the same defaults (one 1.44 MB drive, equipment
      21h, configuration byte A 00h) without writing them. On a dead
      battery the time is still reset to 2010-01-01 00:00 as upstream;
    - the co-processor bit of the equipment word comes from the detection
      only; upstream ORs the CMOS bit into it;
    - `cmos_check` runs after the memory size and the EBDA and before the
      extension ROM scan. It keeps the base memory size POST found
      (0040:0013h plus the EBDA this BIOS reserved) in the BIOS data area
      word 0040:0015h for the setup; that word is IBM's POST-only
      MFG_ERR_FLAG, which this BIOS does not otherwise use. With a valid
      CMOS it sets 20h if the equipment byte (14h) does not match the
      hardware: the diskette bits (0, 7–6) against the drive types in
      10h, as `nvram_save` sets them (this BIOS takes the drives from 10h
      and does not probe them; with only the second drive set, one drive,
      as unemupc98 counts it, is accepted as well as two), the
      co-processor bit against the detection, and the display type: with
      a video ROM (AA55h at C0000h–C7FFFh, 2 KiB steps) anything but CGA
      (10h, 20h), without one monochrome (30h) exactly when the display
      switch selects the monochrome adapter. It sets 10h if the base
      memory in 15h–16h (bits 7–6 of 16h ignored) differs from the size
      POST found, or the extended memory in 17h–18h from 30h–31h. It
      always sets 04h if the RTC stays in its update cycle (about 4000
      reads of register A) or a time or date register (00h, 02h, 04h,
      07h–09h, 32h) is not BCD within its range;
    - then 0Eh is written and each error is printed, IBM's numbers with
      this BIOS's wording. `boot_os`, after its F1 check, shows POST code
      28h, discards the keys typed so far, prints the prompt and waits for
      F1 (continue) or F2 (run the BIOS setup); if F1 was pressed during
      the POST, the setup runs without the wait. A valid CMOS prints
      nothing and adds no wait:

      ```
      161-CMOS battery failed
      162-CMOS checksum or configuration error
      163-Time and date not set
      164-Memory size error
      Press F1 to continue, F2 to run BIOS setup
      ```
    - the BIOS setup (`nvram_save`) also writes the base memory size kept
      in 0040:0015h to 15h–16h, 30h–31h to 17h–18h, the display type 00h
      when a video ROM is present, and 00h to 0Eh. After a 161 or 162 it
      first clears 11h–13h and 19h–2Ch, so that the new checksum does not
      cover stale bytes such as the fixed disk types (upstream clears
      10h–20h on a bad checksum);
    - differences from the IBM AT: a configuration without a diskette
      drive, the display type 00h without a video ROM (no display) and
      monochrome with a video ROM (two adapters) are accepted, as
      unemupc98 derives them; 14h bits 7–6 are compared with the drive
      types in 10h, with the tolerance above (IBM compares the second
      drive type with the drives it finds); after a configuration error
      (20h) the CMOS equipment byte is still used (IBM falls back to a
      minimum configuration when its display or diskette check fails);
      the clock check runs on every POST, also after 161 or 162 (IBM
      skips it then), and does not wait for an update cycle (the IBM POST
      waits up to about a second for one); there are no error beeps; F2
      at the prompt runs the BIOS setup;
  - banner (`src/messages.inc`): "unemu AT BIOS, Version 1.0.2-unemu.2",
    Sergey Kiselev's copyright line unchanged, and a line crediting the
    unemu contributors. `VERSION` is overridden for this configuration only;
    the ROM date stays upstream's.

## Plan

The `MACHINE_UNEMU_AT` configuration targets an IBM PC/AT-compatible board
(80286, two 8259s, two 8237s, 8042, MC146818). It has the CMOS shutdown-byte
resume after a processor reset (`AT_SHUTDOWN`: codes 05h, 09h and 0Ah, and
04h through the POST), INT 15h AH=87h and AH=88h, with the extended
memory size measured at POST and A20 disabled after POST
(`AT_EXT_MEMORY`), and the CMOS configuration checks with the checksum
over 10h–2Dh and the 161–164 messages (`AT_CMOS_CHECK`).

The work is tracked in this repository's beads (prefix `uab`). unemupc98 fetches
released images by SHA-256 and lists them in its firmware catalogue as open
firmware.
