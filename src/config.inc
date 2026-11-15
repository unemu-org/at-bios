;=========================================================================
; config.inc - Compilation time settings and settings
;-------------------------------------------------------------------------
;
; Compiles with NASM 2.13.02, might work with other versions
;
; Copyright (C) 2010 - 2026 Sergey Kiselev.
; Provided for hobbyist use on the Xi 8088 and Micro 8088 boards.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
;=========================================================================

%define DATE		'03/27/26'	; BIOS release date MM/DD/YY
%define VERSION		'1.0.2'		; BIOS version

; Machine type is defined in the Makefile
;%define MACHINE_XI8088			; Xi 8088
;%define MACHINE_FE2010A 		; Faraday FE2010A
;%define MACHINE_BOOK8088		; Book8088
;%define MACHINE_HOMEBREW8088		; EMM Computers "Homebrew8088" board
;%define MACHINE_XT			; IBM PC/XT or highly compatible board
					; FIXME: not implemented yet
;%define MACHINE_UNEMU_AT		; unemu AT (emulated IBM PC/AT board)

; Settings for Xi 8088
%ifdef MACHINE_XI8088
%define	START		8000h		; BIOS starts at offset 8000h
%define MODEL_BYTE	0FCh		; Pretend it is an IBM PC/AT
%define AT_RTC				; Has AT-compatible RTC
%define AT_RTC_NVRAM			; Use AT RTC for BIOS configuration
%define AT_NMI				; Use port 70h for NMI enable/disable
%define AT_DELAY			; Use refresh bit of port 61h for delays
%define SECOND_PIC			; Has two PICs (AT-style)
%define AT_KEYBOARD			; Has AT-compatible keyboard controller
%define PS2_MOUSE			; Enable PS/2 auxiliary device support
%define DISABLE_KBD_DURING_INTERRUPTS	; Don't disable keyboard in INT1
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define EBDA_SIZE	1		; 1KB reserved for EBDA
					; EBDA is required for PS/2 aux support
%endif ; MACHINE_XI8088

; Settings for the unemu AT board (emulated IBM PC/AT-compatible 80286 board)
; Same as Xi 8088, except: no TURBO_MODE (on the AT, bit 2 of port 61h
; disables the RAM parity check, it is not a turbo switch), its own VERSION,
; FLOPPY_RESET_ANY_DRIVE, AT_DISPLAY_SWITCH, AT_CPU, AT_SHUTDOWN, AT_DMA,
; AT_EXT_MEMORY, AT_NMI_POLARITY and AT_CMOS_CHECK. See README.unemu.md for
; all differences
%ifdef MACHINE_UNEMU_AT
%define VERSION		'1.0.2-unemu.3'	; unemu AT BIOS version
%define	START		8000h		; BIOS starts at offset 8000h
%define MODEL_BYTE	0FCh		; IBM PC/AT
%define AT_RTC				; Has AT-compatible RTC
%define AT_RTC_NVRAM			; Use AT RTC for BIOS configuration
%define AT_NMI				; Use port 70h for NMI enable/disable
%define AT_DELAY			; Use refresh bit of port 61h for delays
%define SECOND_PIC			; Has two PICs (AT-style)
%define AT_KEYBOARD			; Has AT-compatible keyboard controller
%define PS2_MOUSE			; Enable PS/2 auxiliary device support
%define DISABLE_KBD_DURING_INTERRUPTS	; Don't disable keyboard in INT1
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define EBDA_SIZE	1		; 1KB reserved for EBDA
					; EBDA is required for PS/2 aux support
%define FLOPPY_RESET_ANY_DRIVE		; INT 13h AH=00h accepts any drive
%define AT_DISPLAY_SWITCH		; IBM PC/AT display switch polarity
%define AT_CPU				; Has 80286 CPU and 80287 FPU socket
%define AT_SHUTDOWN			; CMOS shutdown byte resume
%define AT_DMA				; Has two DMACs (AT-style)
%define AT_EXT_MEMORY			; INT 15h AH=87h, AH=88h
%define AT_NMI_POLARITY			; IBM PC/AT port 70h NMI mask polarity
%define AT_CMOS_CHECK			; IBM PC/AT CMOS configuration checks
%endif ; MACHINE_UNEMU_AT

; Settings for FE2010A
%ifdef MACHINE_FE2010A
%define	START		0A000h		; BIOS starts at offset 0A000h
%define MODEL_BYTE	0FEh		; IBM PC/XT
%define AT_RTC				; Has AT-compatible RTC
%define AT_RTC_AUTODETECT		; Autodetect RTC presence
%define FLASH_NVRAM			; Use Flash ROM for BIOS configuration
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define PIT_DELAY			; Use PIT polling for delays
%endif ; MACHINE_FE2010A

; Settings for Book8088
%ifdef MACHINE_BOOK8088
%define	START		0C000h		; BIOS starts at 0C000h - 16 KiB total
%define MODEL_BYTE	0FEh		; IBM PC/XT
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define PIT_DELAY			; Use PIT polling for delays
%endif ; MACHINE_BOOK8088

; Settings for IBM PC/XT
%ifdef MACHINE_XT
;%define	START		0E000h		; BIOS starts at offset 0E000h
%define	START		0C000h		; FIXME: more ROM for development
%define MODEL_BYTE	0FEh		; IBM PC/XT
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define PIT_DELAY			; Use PIT polling for delays
%endif ; MACHINE_XT

%ifdef MACHINE_HOMEBREW8088
%define	START		0C000h		; FIXME: more ROM for development
%define MODEL_BYTE	0FEh		; IBM PC/XT
%define MIN_RAM_SIZE	32		; At least 32 KiB to boot the system
%define MAX_RAM_SIZE	640		; Scan this much memory during POST
%define RAM_TEST_BLOCK	16384		; block size for RAM test
%define PIT_DELAY			; Use PIT polling for delays
%define AT_KEYBOARD			; Has AT-compatible keyboard controller
%define DISABLE_KBD_DURING_INTERRUPTS
; Optional: Enable DS12885/DS12887 based RTC
;%define AT_RTC				; Has AT-compatible RTC
;%define AT_RTC_AUTODETECT		; Autodetect RTC presence
; Specify RTC I/O address if different from the default 0x70
;%define AT_RTC_PORT	2A0h
%endif ; MACHINE_HOMEBREW8088

; Automatic settings based on the machine settings above
%ifdef AT_RTC or AT_RTC_NVRAM or FLASH_NVRAM
%define BIOS_SETUP			; Include BIOS setup utility
%endif ; AT_RTC or AT_RTC_NVRAM or FLASH_NVRAM

; Note: while Book8088 does have a turbo mode, it is not software controlled
%ifdef MACHINE_XI8088 or MACHINE_FE2010A
%define TURBO_MODE			; Has turbo mode
%endif ; MACHINE_XI8088 or MACHINE_FE2010A

; Default floppy drives configuration for systems without NVRAM/Flash ROM setup
; First digit = drive A, second = drive B
; 0 - none,	1 - 360 KB,	2 - 1.2 MB
; 3 - 720 KB,	4 - 1.44 MB,	6 - 2.88 MB
; Use two 1.44 MB drives by default
%define DEFAULT_FLOPPIES 44h

; Check for the boot sector signature
; Note: Most BIOSes do not check for it
; %define BOOT_SIGNATURE

; INT 13h AH=00h (reset disk system) accepts any drive number in DL,
; including fixed disk reset requests, as the IBM PC/AT BIOS does
; Note: MACHINE_UNEMU_AT sets it
; %define FLOPPY_RESET_ANY_DRIVE

; Read the display type switch (keyboard controller input port bit 6) with
; the IBM PC/AT polarity: 0 = color adapter, 1 = monochrome adapter
; The Xi 8088 switch SW2-8 reads the other way: 0 (on) = MDA, 1 (off) = CGA
; Note: MACHINE_UNEMU_AT sets it
; %define AT_DISPLAY_SWITCH

; The board has an 80286 CPU: POST detects 80286 and 80386 or later CPUs
; before the 8088 and V20 tests, accepts the co-processor control word
; 037Fh (80287XL, 80387) as well as 03FFh after FNINIT, and reports the
; co-processor as 80287 (an 8087 cannot be used with an 80286). The
; detection code uses 8086 instructions only
; Note: MACHINE_UNEMU_AT sets it
; %define AT_CPU

; After a soft reset (a CPU reset through the 8042 keyboard controller or
; a triple fault, told from a power on by the 8042 system flag), act on the
; CMOS shutdown status byte (0Fh) as the IBM PC/AT BIOS does: codes 05h and
; 0Ah (far jump through 0040:0067h), 09h (block move return with
; AT_EXT_MEMORY, a cold start without it), others - POST. Code 04h runs
; the POST, which then boots the OS (the IBM PC/AT BIOS goes to INT 19h
; directly). Power on and the IOCHK NMI reboot clear the shutdown byte and
; cold start. The warm boot flag at 0040:0072h is cleared at power on and
; at the end of POST, so 1234h skips the memory test once. Uses the IBM
; PC/AT port 70h NMI polarity (bit 7 set masks NMI). Requires AT_KEYBOARD,
; AT_RTC and SECOND_PIC
; Note: MACHINE_UNEMU_AT sets it
; %define AT_SHUTDOWN

; The board has a second DMAC (8237, channels 4-7) at ports C0h-DFh: POST
; initializes it as the IBM PC/AT BIOS does, with channel 4 (the cascade
; from the first DMAC, channels 0-3) in cascade mode and unmasked, and
; channels 5-7 masked. The port written for hardware detection
; and I/O delays (unused_reg) moves from 0C0h, which is the second DMAC's
; channel 4 address register there, to 8Eh, a DMA page register the AT does
; not use (and not 80h, the POST code port)
; Note: MACHINE_UNEMU_AT sets it
; %define AT_DMA

; INT 15h AH=87h (move block) and AH=88h (get extended memory size) as in
; the IBM PC/AT BIOS. AH=88h returns CMOS 30h-31h in AX, flags unchanged.
; AH=87h moves CX words between the source and target descriptors (24-bit
; bases) of the caller's GDT at ES:SI in 80286 protected mode, with A20
; enabled through the keyboard controller, then resets the CPU through the
; keyboard controller with the CMOS shutdown code 09h, disables A20 and
; returns with AH = status (CF = 1 and ZF = 0 on an error). As the IBM
; PC/AT POST does, POST measures the memory above 1 MiB in protected mode,
; in 64 KiB blocks up to the first one missing (at most 15 MiB), through
; the same protected mode entry and CPU reset, and stores the size in
; 30h-31h. Both need the AT's CPU-only reset through the keyboard
; controller; where that reset is a full system reset, the next POST
; reports the configured size (CMOS 17h-18h) instead and boots. POST
; disables A20 at the end of the keyboard controller initialization,
; as the IBM PC/AT POST does. Uses 80286 instructions. Requires AT_SHUTDOWN
; and AT_CPU
; Note: MACHINE_UNEMU_AT sets it
; %define AT_EXT_MEMORY

; Port 70h bit 7 (NMI mask) has the IBM PC/AT polarity: 1 masks NMI, 0
; enables it. The Xi 8088 uses the reverse: 0 disables NMI, 1 enables it.
; POST masks NMI at the start and enables it after initializing the PICs and
; the keyboard controller. The NMI handler masks NMI and enables it again on
; exit, as the IBM PC/AT BIOS does after an NMI that is not an I/O check;
; after an I/O check, key 'd' leaves it masked until the next rtc_read or
; rtc_write with bit 7 clear (INT 1Ah, IRQ8). rtc_read and rtc_write mask
; NMI during the access, then select register 0Dh with bit 7 of the address
; as the NMI mask, as the IBM PC/AT CMOS_READ does (0 = enable NMI).
; AT_SHUTDOWN and AT_EXT_MEMORY write port 70h with this polarity directly.
; Requires AT_NMI and the RTC at port 70h (no AT_RTC_PORT)
; Note: MACHINE_UNEMU_AT sets it
; %define AT_NMI_POLARITY

; Check the CMOS configuration as the IBM PC/AT POST does, and record the
; result in the CMOS diagnostic status byte (0Eh): RTC battery (register D
; VRT bit), checksum of 10h-2Dh in 2Eh (high byte) and 2Fh (low byte), the
; equipment byte (14h: diskettes against 10h, co-processor, display type)
; against the hardware, the memory sizes (15h-16h against the base memory
; found, 17h-18h against 30h-31h), and the RTC time and date. Errors are
; reported as 161-164, then POST shows POST code 28h and waits for F1
; (continue) or F2 (BIOS setup) before booting. The NVRAM checksum covers
; 10h-2Dh instead of 10h-20h. On a dead battery or a bad checksum the CMOS
; is not rewritten: the default floppy, equipment and configuration A
; values are used. The co-processor bit of the equipment word is taken
; from the detection only. The base memory size found is kept in
; 0040:0015h. The BIOS setup also saves the memory sizes, an EGA/VGA
; display type when a video ROM is present, clears 11h-13h and 19h-2Ch
; after a dead battery or a bad checksum, and clears 0Eh. Requires
; AT_RTC_NVRAM and AT_EXT_MEMORY
; Note: MACHINE_UNEMU_AT sets it
; %define AT_CMOS_CHECK
