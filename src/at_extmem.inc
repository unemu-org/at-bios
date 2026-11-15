;=========================================================================
; at_extmem.inc - IBM PC/AT compatible extended memory services
;       INT 15h, functions:
;		87h	- Move block (to or from extended memory)
;		88h	- Get extended memory size
;	POST: ext_mem_size_init - measure the size INT 15h AH=88h reports
;	      bm_a20_set - also used by kbc_init to disable A20
;-------------------------------------------------------------------------
;
; Compiles with NASM 2.13.02, might work with other versions
;
; Copyright (C) 2026 unemu contributors.
; Part of the unemu fork of Sergey Kiselev's 8088_bios.
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

%ifndef AT_SHUTDOWN
%error "AT_EXT_MEMORY depends on AT_SHUTDOWN. Please fix config.inc."
%endif ; AT_SHUTDOWN
%ifndef AT_CPU
%error "AT_EXT_MEMORY depends on AT_CPU (80286). Please fix config.inc."
%endif ; AT_CPU

; The block move runs in 80286 protected mode
	cpu	286

;-------------------------------------------------------------------------
; CMOS locations and keyboard controller commands
cmos_ext_cfg_lo	equ	17h	; configured extended memory in KiB - low
cmos_ext_cfg_hi	equ	18h	; configured extended memory in KiB - high
cmos_ext_mem_lo	equ	30h	; extended memory size in KiB - low byte
cmos_ext_mem_hi	equ	31h	; extended memory size in KiB - high byte
ext_mem_max	equ	3C00h	; 15 MiB, the most an 80286 AT can address
bm_size_end	equ	0FEh	; bm_size stops at FE0000h, where the AT has
				; its ROM alias (at most 0EEh blocks)
bm_shutdown_code equ	09h	; CMOS shutdown code: block move return
bm_reset_wait	equ	2000	; wait for the CPU reset, 30 ms (15 us units)
kbc_cmd_wr_out	equ	0D1h	; write keyboard controller output port
kbc_cmd_reset	equ	0FEh	; pulse output port bit 0 - CPU reset
kbc_out_a20_on	equ	0DFh	; output port value: A20 enabled
kbc_out_a20_off	equ	0DDh	; output port value: A20 disabled
; port 61h bits
bm_ppi_chk	equ	0C0h	; read: RAM parity (7) or ~IOCHK (6) error
bm_ppi_chk_off	equ	0Ch	; write: clear and disable both checks
bm_ppi_keep	equ	03h	; write: speaker bits, left as they are

;-------------------------------------------------------------------------
; The caller's global descriptor table (48 bytes, at ES:SI)
bm_gdt_gdt	equ	08h	; the GDT itself as data (BIOS fills it)
bm_gdt_src	equ	10h	; source segment (caller fills it)
bm_gdt_dst	equ	18h	; target segment (caller fills it)
bm_gdt_code	equ	20h	; BIOS code segment (BIOS fills it)
bm_gdt_stack	equ	28h	; stack segment (BIOS fills it)
bm_gdt_size	equ	30h	; GDT size, bytes
; descriptor fields
bm_desc_limit	equ	0	; word - segment limit
bm_desc_base_lo	equ	2	; word - base address bits 15-0
bm_desc_base_hi	equ	4	; byte - base address bits 23-16
				; byte - access rights (at offset 5)
bm_desc_resvd	equ	6	; word - reserved, must be 0
; access rights bytes
bm_acc_data	equ	93h	; present, DPL 0, data, writable, accessed
bm_acc_code	equ	9Bh	; present, DPL 0, code, readable, accessed
bm_acc_int_gate	equ	86h	; present, DPL 0, 80286 interrupt gate

;-------------------------------------------------------------------------
; The block move frame on the caller's stack (PUSHA, ES, DS)
bm_frame_ds	equ	0
bm_frame_es	equ	2
bm_frame_cx	equ	16
bm_frame_ax	equ	18	; bm_size: AL = 64 KiB blocks, AH = status
bm_frame_ah	equ	19	; the status, returned in AH
; the memory size test (bm_size)
bm_size_pat0	equ	0AA55h	; test word at the start of a block
bm_size_pat1	equ	55AAh	; test word at the end of a block
bm_size_mark	equ	0FFh	; CMOS 31h while the sizing runs (above 3Ch)

;=========================================================================
; ext_mem_size_init - measure the extended memory and store its size in
;	CMOS 30h-31h, which INT 15h AH=88h reports
; Input:
;	none, called from the POST after rtc_init, with the stack, the BIOS
;	data area and the PICs set up
; Output:
;	AX = extended memory size in KiB, 0 to 3C00h (15 MiB), also stored
;	in CMOS 30h (low) and 31h (high)
;	interrupts enabled, A20 disabled, other registers preserved
; Notes:
;	- As the IBM PC/AT POST does, the memory above 1 MiB is sized in
;	  80286 protected mode in 64 KiB blocks, up to the first block that
;	  does not keep the test words (bm_size). The protected mode entry,
;	  A20 and the return through a CPU reset with the CMOS shutdown code
;	  09h are those of INT 15h AH=87h (bm_run), so each POST resets the
;	  CPU once more. Code 09h leaves the PICs as they are
;	- The GDT is on the stack, reserved below the block move frame. The
;	  result comes back in the frame: AL = blocks, AH = status. An error
;	  (an exception in protected mode, a keyboard controller timeout)
;	  gives size 0
;	- The memory found above 1 MiB is cleared. Below 1 MiB only the
;	  stack and 0040:0067h-0040:006Ah (the frame pointer across the
;	  reset) change
;	- The sizing needs the AT's CPU-only reset through the keyboard
;	  controller. While it runs, CMOS 31h holds FFh, more than any size.
;	  Where the reset is a full system reset instead (the keyboard
;	  controller and CMOS 0Fh cleared), it comes back as a power on, and
;	  the next POST finds FFh: it then skips the sizing and reports the
;	  configured size (CMOS 17h-18h, at most 15 MiB), which also clears
;	  the mark, so the machine boots instead of resetting in a loop
;	- The comparison with the configured size (CMOS 17h-18h) is not
;	  done here
;	- 30h-31h are outside the checksummed CMOS range, no checksum update
;	- NMI is masked while CMOS is accessed, then register 0Dh is selected
;	  with NMI enabled (AT polarity), as rtc_init leaves bit 7 clear
;-------------------------------------------------------------------------
ext_mem_size_init:
	push	bx
	push	cx
	push	dx
	push	si
	push	es
	mov	al,cmos_ext_mem_hi | cmos_nmi_mask
	call	rtc_read
	cmp	al,bm_size_mark		; did the last sizing reset the system?
	je	.configured		; yes, do not size again
	mov	ax,(bm_size_mark << 8) | cmos_ext_mem_hi | cmos_nmi_mask
	call	rtc_write		; mark the sizing
	sub	sp,bm_gdt_size		; the GDT, on the stack
	mov	si,sp
	push	ss
	pop	es			; ES:SI = the GDT
	xor	ax,ax			; base 100000h, the first block
	mov	dx,(bm_acc_data << 8) | 10h
	mov	cx,0FFFFh
	mov	bx,bm_gdt_dst
	call	bm_set_desc		; target: bm_size moves it per block
	pushf				; called as INT 15h AH=87h is:
	push	cs			; bm_run returns with RETF 2
	call	.run			; AL = blocks, AH = status, CF = error
	jnc	.size
	xor	ax,ax			; an error: no extended memory
.size:
	mov	ah,64
	mul	ah			; AX = size in KiB
	add	sp,bm_gdt_size
	mov	bx,ax
.store:
	mov	al,cmos_ext_mem_lo | cmos_nmi_mask
	mov	ah,bl
	call	rtc_write
	mov	al,cmos_ext_mem_hi | cmos_nmi_mask
	mov	ah,bh
	call	rtc_write
	mov	al,cmos_control_d	; enable NMI (AT polarity)
	out	rtc_addr_reg,al
	mov	ax,bx			; AX = size in KiB
	pop	es
	pop	si
	pop	dx
	pop	cx
	pop	bx
	ret

.configured:
	mov	al,cmos_ext_cfg_hi | cmos_nmi_mask
	call	rtc_read
	mov	bh,al
	mov	al,cmos_ext_cfg_lo | cmos_nmi_mask
	call	rtc_read
	mov	bl,al			; BX = configured size, KiB
	cmp	bx,ext_mem_max
	jbe	.store
	mov	bx,ext_mem_max
	jmp	.store

.run:
	cld
	pusha				; the block move frame
	mov	di,bm_size		; the work in protected mode: sizing
	jmp	bm_run

;=========================================================================
; int_15_fn88 - Get extended memory size
; Input:
;	AH = 88h
; Output:
;	AX = extended memory size in KiB, from CMOS 30h (low) and 31h (high)
;	Flags are the caller's, unchanged, as in the IBM PC/AT BIOS
; Notes:
;	- NMI is masked while CMOS is read, then enabled with register 0Dh
;	  selected. On the AT, bit 7 of port 70h set masks NMI
;	- POST sets 30h-31h (ext_mem_size_init)
;-------------------------------------------------------------------------
int_15_fn88:
	mov	al,cmos_ext_mem_hi | cmos_nmi_mask
	call	rtc_read
	mov	ah,al
	mov	al,cmos_ext_mem_lo | cmos_nmi_mask
	call	rtc_read
	push	ax
	mov	al,cmos_control_d	; enable NMI (AT polarity)
	out	rtc_addr_reg,al
	pop	ax
	iret

;=========================================================================
; int_15_fn87 - Move block
; Input:
;	AH = 87h
;	CX = number of words to move (up to 8000h)
;	ES:SI = the caller's 48-byte global descriptor table:
;		00h	- dummy descriptor
;		08h	- filled by the BIOS: the GDT as data
;		10h	- source: limit, 24-bit base, access rights 93h
;		18h	- target: limit, 24-bit base, access rights 93h
;		20h	- filled by the BIOS: BIOS code segment
;		28h	- filled by the BIOS: stack segment
; Output:
;	AH = status:
;		00h	- success, CF = 0, ZF = 1
;		01h	- RAM parity or I/O channel check error, CF = 1, ZF = 0
;		02h	- exception in protected mode (also a triple fault),
;			  CF = 1, ZF = 0
;		03h	- keyboard controller timeout (A20), CF = 1, ZF = 0
;	Other registers preserved, interrupts enabled
; Notes:
;	- The move is done in protected mode with interrupts disabled and
;	  NMI masked, A20 enabled through the keyboard controller. The CPU
;	  returns to real mode through a CPU reset by the keyboard
;	  controller, with the CMOS shutdown code 09h: shutdown_check then
;	  goes to block_move_return
;	- The caller's registers are saved on its stack, and its SS:SP is
;	  kept at 0040:0069h (SS) and 0040:0067h (SP) across the reset
;	- The status is kept across the reset in the saved AH on the
;	  caller's stack: RAM survives the reset, and in protected mode the
;	  stack segment (selector 28h, or the cached real mode SS before it
;	  is loaded) still addresses it through BP. It is 02h from before
;	  protected mode until the move is done, so a triple fault, which
;	  also resets the CPU, returns an error
;	- Returns with RETF 2, as the IBM PC/AT BIOS does: the flags are set
;	  from the status, and the caller's saved flags are dropped
;	- bm_run is the protected mode round trip without the move: DI is the
;	  work to do in protected mode (bm_move, or bm_size for the POST)
;-------------------------------------------------------------------------
int_15_fn87:
	cld
	pusha				; save the general registers
	mov	di,bm_move		; the work in protected mode: the move
bm_run:
	push	es
	push	ds
	mov	bp,sp			; BP = the block move frame
	cli
	mov	ax,biosdseg
	mov	ds,ax
	mov	[67h],sp		; keep the caller's SS:SP in
	mov	[69h],ss		; 0040:0067h across the reset

; fill the BIOS descriptors in the caller's GDT

	mov	ax,es
	mov	dx,si
	call	bm_linear		; DL:AX = the GDT address
	mov	dh,bm_acc_data
	mov	cx,bm_gdt_size-1
	mov	bx,bm_gdt_gdt
	call	bm_set_desc		; GDT as data

	mov	ax,ss
	xor	dx,dx
	call	bm_linear		; DL:AX = the caller's stack segment
	mov	dh,bm_acc_data
	mov	cx,0FFFFh
	mov	bx,bm_gdt_stack
	call	bm_set_desc		; stack segment

	xor	ax,ax			; base 0F0000h
	mov	dx,(bm_acc_code << 8) | (bioscseg >> 12)
	mov	cx,0FFFFh
	mov	bx,bm_gdt_code
	call	bm_set_desc		; BIOS code segment

; enable A20

	mov	ah,kbc_out_a20_on
	call	bm_a20_set
	jnc	.a20_on
	mov	byte [bp+bm_frame_ah],03h ; keyboard controller timeout
	jmp	block_move_return

.a20_on:
	mov	al,cmos_shutdown | cmos_nmi_mask
	out	rtc_addr_reg,al		; mask NMI, select the shutdown byte
	jmp	$+2
	mov	al,bm_shutdown_code	; return through block_move_return
	out	rtc_data_reg,al		; after the CPU reset
	mov	byte [bp+bm_frame_ah],02h ; an error until the move is done,
					; in case an exception cannot be
					; delivered (triple fault)

; switch to protected mode

	lidt	[cs:bm_idt_ptr]		; exceptions go to bm_exception
	lgdt	[es:si+bm_gdt_gdt]	; the caller's GDT
	smsw	ax
	or	al,01h			; PE = 1
	lmsw	ax
	jmp	bm_gdt_code:bm_protected

bm_protected:
	mov	ax,bm_gdt_stack
	mov	ss,ax			; same stack, SP unchanged
	jmp	di			; bm_move or bm_size

bm_move:
	mov	ax,bm_gdt_src
	mov	ds,ax
	mov	ax,bm_gdt_dst
	mov	es,ax
	mov	cx,[bp+bm_frame_cx]	; number of words
	xor	si,si
	xor	di,di
	rep	movsw			; move the block
	mov	byte [bp+bm_frame_ah],00h ; done, no error so far

; check for a RAM parity or I/O channel check error during the move

	in	al,ppi_pb_reg
	test	al,bm_ppi_chk
	jz	bm_reset
	mov	byte [bp+bm_frame_ah],01h ; parity error
bm_clear_checks:
	and	al,bm_ppi_keep
	or	al,bm_ppi_chk_off
	out	ppi_pb_reg,al		; clear and disable the checks
	and	al,bm_ppi_keep
	out	ppi_pb_reg,al		; enable the checks again
	jmp	bm_reset

;-------------------------------------------------------------------------
; bm_size - the work of ext_mem_size_init: count the 64 KiB blocks of
;	memory from 1 MiB up, to the first block that does not keep two
;	different test words, one at its start and one at its end, or to
;	FE0000h, as the IBM PC/AT POST does: the AT decodes its ROM at
;	FE0000h-FFFFFFh. AL in the frame = the number of blocks, AH = 00h
; Notes:
;	- The block move target descriptor (18h) is moved to each block in
;	  turn through the GDT data descriptor (08h). Its base is 24 bits, so
;	  the blocks do not wrap below 16 MiB
;	- Two different words, read back after both are written, fail on a
;	  bus that reads 00h or FFh, or the last value written
;	- Each block found is cleared (zeroed), on every POST, as the IBM
;	  PC/AT POST does: memory above 1 MiB keeps nothing from the previous
;	  boot, such as the VDISK header at 100000h that DOS kernels and XMS
;	  drivers use to tell whether extended memory is already taken
;	- A parity or I/O channel check error that reading an empty block
;	  latches is cleared and not reported

bm_size:
	mov	ax,bm_gdt_gdt
	mov	ds,ax			; DS = the GDT, to move the target
	mov	bx,bm_gdt_dst
	mov	dl,10h			; the first block, at 1 MiB
.block:
	mov	[bm_gdt_dst+bm_desc_base_hi],dl ; block base = DL * 64 KiB
	mov	es,bx			; ES = the block
	mov	si,[es:0]		; keep the words the test changes
	mov	di,[es:0FFFEh]
	mov	word [es:0],bm_size_pat0
	mov	word [es:0FFFEh],bm_size_pat1
	mov	ax,[es:0]		; read both back
	mov	cx,[es:0FFFEh]
	mov	[es:0],si		; put the old words back
	mov	[es:0FFFEh],di
	xor	ax,bm_size_pat0
	xor	cx,bm_size_pat1
	or	ax,cx
	jnz	.done			; no memory here
	xor	ax,ax			; clear the block, as the IBM POST
	xor	di,di			;   does, so nothing left in it by
	mov	cx,8000h		;   the previous boot (such as a
	rep	stosw			;   VDISK header at 1 MiB) survives
	inc	dl			; next block
	cmp	dl,bm_size_end
	jb	.block			; up to FE0000h
.done:
	sub	dl,10h			; DL = blocks found, 0EEh at most
	mov	dh,00h			; status: done
	mov	[bp+bm_frame_ax],dx	; AL = blocks, AH = status
	in	al,ppi_pb_reg
	jmp	bm_clear_checks

;-------------------------------------------------------------------------
; bm_exception - any exception in protected mode: status 02h, CPU reset
; Note: SS:BP still addresses the block move frame

bm_exception:
	mov	byte [bp+bm_frame_ah],02h ; exception in protected mode

;-------------------------------------------------------------------------
; bm_reset - back to real mode through a CPU reset by the keyboard
;	controller. The CMOS shutdown code 09h leads to block_move_return.
;	If the CPU still runs after a while, a triple fault (an interrupt
;	with an empty IDT) resets it instead

bm_reset:
	call	kbc_wait_write
	mov	al,kbc_cmd_reset
	out	kbc_command_reg,al	; reset the CPU
	mov	cx,bm_reset_wait
	call	delay_15us
	lidt	[cs:bm_idt_none]
	int	3			; triple fault
.halt:
	hlt
	jmp	.halt

;=========================================================================
; block_move_return - return from INT 15h AH=87h to its caller
; Input:
;	DS = BIOS data area, interrupts disabled
;	0040:0067h = SP, 0040:0069h = SS of the block move frame
;	Entered from shutdown_check (shutdown code 09h, NMI masked, the
;	CMOS shutdown byte cleared), or from bm_run when A20 cannot be
;	enabled
; Output:
;	returns to the INT 15h caller, see int_15_fn87 (or to
;	ext_mem_size_init, which calls bm_run the same way)
;-------------------------------------------------------------------------
block_move_return:
	mov	ss,[69h]		; the caller's stack
	mov	sp,[67h]
	mov	bp,sp			; BP = the block move frame
	mov	ah,kbc_out_a20_off
	call	bm_a20_set		; disable A20
	jnc	.a20_off
	cmp	byte [bp+bm_frame_ah],00h
	jne	.a20_off		; keep the earlier error
	mov	byte [bp+bm_frame_ah],03h ; keyboard controller timeout

.a20_off:
	mov	al,cmos_control_d	; enable NMI (AT polarity)
	out	rtc_addr_reg,al
	mov	al,[bp+bm_frame_ah]
	neg	al			; status 0: CF = 0, ZF = 1
					; otherwise CF = 1, ZF = 0
	pop	ds			; the flags are kept from here on
	pop	es
	popa				; AH = status
	sti
	retf	2			; return with these flags

;=========================================================================
; bm_a20_set - set the keyboard controller output port (A20 gate)
; Input:
;	AH = output port value (kbc_out_a20_on or kbc_out_a20_off)
; Output:
;	CF = 0 - done, the controller took the data
;	CF = 1 - keyboard controller timeout
;	AL trashed
;-------------------------------------------------------------------------
bm_a20_set:
	mov	al,kbc_cmd_wr_out
	call	kbc_send_cmd		; command, then AH as data
	jc	.exit
	call	kbc_wait_write		; wait until the data is taken
.exit:
	ret

;=========================================================================
; bm_linear - real mode segment and offset to a 24-bit address
; Input:
;	AX = segment
;	DX = offset
; Output:
;	AX = address bits 15-0
;	DL = address bits 23-16, DH = 0
;-------------------------------------------------------------------------
bm_linear:
	push	bx
	mov	bx,ax
	shl	ax,4			; segment bits 11-0 to address 15-4
	shr	bx,12			; segment bits 15-12 to address 19-16
	add	ax,dx
	adc	bl,0
	mov	dx,bx
	pop	bx
	ret

;=========================================================================
; bm_set_desc - fill a descriptor in the caller's GDT
; Input:
;	ES:SI = GDT
;	BX = descriptor offset (selector)
;	AX = base address bits 15-0
;	DL = base address bits 23-16
;	DH = access rights byte
;	CX = segment limit
; Output:
;	none
;-------------------------------------------------------------------------
bm_set_desc:
	mov	[es:si+bx+bm_desc_limit],cx
	mov	[es:si+bx+bm_desc_base_lo],ax
	mov	[es:si+bx+bm_desc_base_hi],dx	; base 23-16, access rights
	mov	word [es:si+bx+bm_desc_resvd],0
	ret

;-------------------------------------------------------------------------
; Interrupt descriptor table used during the block move: every exception
; vector (00h - 1Fh) goes to bm_exception. Hardware interrupts are
; disabled and NMI is masked during the move

bm_idt_ptr:
	dw	bm_idt_end-bm_idt-1	; limit
	dw	bm_idt			; base bits 15-0 (in segment 0F000h)
	db	bioscseg >> 12		; base bits 23-16
	db	0

bm_idt:
	times 32 dw bm_exception, bm_gdt_code, bm_acc_int_gate << 8, 0
bm_idt_end:

bm_idt_none:				; empty IDT, for the triple fault
	dw	0, 0, 0

	cpu	8086
