;=========================================================================
; at_shutdown.inc - IBM PC/AT compatible soft reset handling: the CMOS
;	shutdown status byte dispatch after a CPU reset through the keyboard
;	controller or a triple fault
;-------------------------------------------------------------------------
;
; Compiles with NASM 2.13.02, might work with other versions
;
; Copyright (C) 2026 unemu contributors.
; Part of the unemu fork of Sergey Kiselev's 8088_bios.
; The 8259 initialization sequence follows the one in bios.asm, by
; Sergey Kiselev.
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

;-------------------------------------------------------------------------
; shutdown_check - tell a power on from a soft reset (a CPU reset through
;	the keyboard controller or a triple fault), and after a soft reset
;	act on the CMOS shutdown status byte, as the IBM PC/AT BIOS does
; Input:
;	none, entered from cold_start without a valid stack
; Output:
;	power on	- shutdown byte cleared, cold start (warm boot flag
;			  cleared)
;	soft reset	- CMOS shutdown byte cleared, then by its code:
;		05h	- PICs initialized and masked, EOI, keyboard
;			  controller output buffer flushed, far jump
;			  through 0040:0067h
;		09h	- return from INT 15h AH=87h (block move)
;		0Ah	- far jump through 0040:0067h, PICs untouched
;		others	- PICs initialized, POST, warm boot flag kept
; Notes:
;	- No stack operations and no RAM writes before SS:SP is set, and
;	  none at all on the 05h and 0Ah paths. The program resumed through
;	  0040:0067h gets control with interrupts disabled and NMI masked
;	- The 8042 system flag is clear at power on. kbc_init sets it at the
;	  end of the keyboard controller initialization, and a CPU reset
;	  leaves it set
;	- POST clears the warm boot flag before it boots the OS (boot_os),
;	  so 1234h there skips the memory test once
;	- Code 04h runs the POST, which then boots the OS. The IBM PC/AT BIOS
;	  goes to INT 19h directly, but here both PICs are masked by then,
;	  and the floppy code would wait for IRQ6 forever
;	- On the AT, bit 7 of port 70h set masks NMI. This code writes port
;	  70h directly, with that polarity
;-------------------------------------------------------------------------
kbc_stat_sys	equ	04h	; 8042 status: system flag (0 = power on)
cmos_shutdown	equ	0Fh	; CMOS shutdown status byte
cmos_nmi_mask	equ	80h	; port 70h bit 7 set masks NMI (AT polarity)

shutdown_check:
	cli
	mov	al,cmos_shutdown | cmos_nmi_mask
	out	rtc_addr_reg,al		; mask NMI, select the shutdown byte
	in	al,kbc_status_reg
	test	al,kbc_stat_sys		; system flag set?
	jz	shutdown_cold		; no, power on - cold start

	in	al,rtc_data_reg		; read the shutdown byte
	mov	ah,al			; AH = shutdown code
	cmp	ah,09h			; block move return?
	je	.clear_code		; leave the PICs as they are
	cmp	ah,0Ah			; jump without initialization?
	je	.clear_code		; leave the PICs as they are
	mov	al,00h
	out	0F1h,al			; reset the math coprocessor
	mov	al,11h			; ICW1 - edge triggered, cascade, ICW4
	out	pic1_reg0,al
	out	pic2_reg0,al
	mov	al,8			; ICW2 - interrupt vector offset = 8
	out	pic1_reg1,al
	mov	al,70h			; ICW2 - interrupt vector offset = 70h
	out	pic2_reg1,al
	mov	al,4			; ICW3 - slave is connected to IR2
	out	pic1_reg1,al
	mov	al,2			; ICW3 - slave ID = 2 (IR2)
	out	pic2_reg1,al
	mov	al,1			; ICW4 - 8086/8088
	out	pic1_reg1,al
	out	pic2_reg1,al
	mov	al,0FFh			; OCW1 - mask all interrupts
	out	pic1_reg1,al
	out	pic2_reg1,al

.clear_code:
	mov	al,cmos_shutdown | cmos_nmi_mask
	out	rtc_addr_reg,al		; NMI stays masked
	jmp	$+2
	mov	al,00h			; clear the shutdown byte, so that
	out	rtc_data_reg,al		; another reset runs the POST
	mov	bx,biosdseg
	mov	ds,bx			; DS = BIOS data area
	xor	bx,bx
	mov	ss,bx			; temporary stack at 0000:0400h
	mov	sp,0400h
	cmp	ah,05h
	je	shutdown_eoi
	cmp	ah,0Ah
	je	shutdown_jump
	cmp	ah,09h
	je	shutdown_block_move
	jmp	warm_start		; others - POST, keep warm boot flag

;-------------------------------------------------------------------------
; shutdown code 05h - PICs initialized above, flush the keyboard controller
; output buffer, EOI, far jump through 0040:0067h

shutdown_eoi:
	in	al,kbc_data_reg		; flush the output buffer
	mov	al,20h			; OCW2 - non-specific EOI
	out	pic1_reg0,al		; to the master PIC

;-------------------------------------------------------------------------
; shutdown code 0Ah - far jump through 0040:0067h

shutdown_jump:
	jmp	far [67h]		; resume the program

;-------------------------------------------------------------------------
; shutdown code 09h - return from the INT 15h AH=87h block move
; Note: a cold start until INT 15h AH=87h is implemented. The block move
;	will save SS:SP at 0040:0067h (SP) and 0040:0069h (SS) before the
;	reset, and this code will restore the stack from there and return
;	to its caller

shutdown_block_move:
	jmp	cold_start_power_on

;=========================================================================
; shutdown_cold - clear the CMOS shutdown byte, then cold start
; Input:
;	none, no stack needed
; Output:
;	does not return: POST with the warm boot flag cleared
; Notes:
;	- Used at power on, so that a code left in CMOS from before cannot
;	  take over the next soft reset, and by the IOCHK NMI reboot in
;	  int_02, which is always a cold start
;-------------------------------------------------------------------------
shutdown_cold:
	cli
	mov	al,cmos_shutdown | cmos_nmi_mask
	out	rtc_addr_reg,al		; mask NMI, select the shutdown byte
	jmp	$+2
	mov	al,00h
	out	rtc_data_reg,al		; clear the shutdown byte
	jmp	cold_start_power_on	; clear the warm boot flag, POST
