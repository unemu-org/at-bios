;=========================================================================
; at_cmos.inc - IBM PC/AT compatible CMOS configuration checks
;	POST:  cmos_validate - battery and checksum status (from rtc_init)
;	       cmos_check - configuration against the hardware, messages
;	       161-164, CMOS diagnostic status byte (0Eh)
;	       cmos_error_wait - F1 to continue after an error (from boot_os)
;	BIOS:  cmos_read_config - configuration byte or its default
;	Setup: cmos_save_config - display type, memory sizes, status byte
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

%ifndef AT_RTC_NVRAM
%error "AT_CMOS_CHECK depends on AT_RTC_NVRAM. Please fix config.inc."
%endif ; AT_RTC_NVRAM
%ifndef AT_EXT_MEMORY
%error "AT_CMOS_CHECK depends on AT_EXT_MEMORY (CMOS 30h-31h). Please fix config.inc."
%endif ; AT_EXT_MEMORY

;-------------------------------------------------------------------------
; CMOS locations
cmos_diag	equ	0Eh	; POST diagnostic status byte
cmos_base_lo	equ	15h	; base memory size in KiB - low byte
cmos_base_hi	equ	16h	; base memory size in KiB - high byte
cmos_base_mask	equ	3Fh	; 16h bits 7-6 are not part of the size
; CMOS 0Eh bits, as in the IBM PC/AT BIOS
cmos_clk_fail	equ	04h	; time or date not valid, or RTC not updating
cmos_hf_fail	equ	08h	; fixed disk failure (not set by this BIOS)
cmos_mem_size	equ	10h	; memory size differs from the configuration
cmos_bad_config	equ	20h	; configuration differs from the hardware
cmos_bad_sum	equ	40h	; checksum of 10h-2Dh not valid
cmos_bad_bat	equ	80h	; RTC lost power, configuration not valid
cmos_invalid	equ	cmos_bad_bat | cmos_bad_sum
; post_flags (BIOS data area 0040:0012h) bit
post_cmos_err	equ	02h	; a CMOS error message was printed
; BIOS data area word: the base memory size POST found, for the BIOS setup
; (IBM's POST-only MFG_ERR_FLAG, not used otherwise by this BIOS)
cmos_post_base	equ	15h
; POST code while waiting for F1 after a CMOS error message
e_cmos_wait	equ	28h
; RTC update in progress: wait at most this many register A reads
cmos_uip_wait	equ	4000

;=========================================================================
; cmos_validate - check the RTC battery and the CMOS checksum, as the IBM
;	PC/AT POST does, and start the CMOS diagnostic status byte
; Input:
;	AL = RTC register D, as read by rtc_init
; Output:
;	CMOS 0Eh = cmos_bad_bat, cmos_bad_sum or 00h. Registers preserved
; Notes:
;	- Register D bit 7 (VRT) clear: the RTC lost power, cmos_bad_bat. The
;	  bit stays set in 0Eh until the BIOS setup saves the configuration
;	  (cmos_save_config clears 0Eh), as in the IBM PC/AT BIOS, where VRT
;	  reads 1 again after the first read of register D
;	- Otherwise the checksum of 10h-2Dh must equal 2Eh (high byte) and 2Fh
;	  (low byte) and must not be 0, or cmos_bad_sum
;	- The configuration is not rewritten: with 0Eh showing an invalid
;	  CMOS, cmos_read_config returns the defaults instead
;-------------------------------------------------------------------------
cmos_validate:
	push	ax
	push	bx
	mov	ah,cmos_bad_bat
	test	al,cmos_vrt
	jz	.write			; the RTC lost power
	mov	al,cmos_diag
	call	rtc_read
	test	al,cmos_bad_bat		; not set up since a power loss?
	jnz	.write			; still not valid
	call	nvram_checksum		; BX = sum of 10h-2Dh
	mov	al,cmos_sum_hi
	call	rtc_read
	mov	ah,al
	mov	al,cmos_sum_lo
	call	rtc_read		; AX = stored checksum
	or	bx,bx
	jz	.bad_sum		; all zero, not configured
	cmp	ax,bx
	mov	ah,00h			; status: valid
	je	.write
.bad_sum:
	mov	ah,cmos_bad_sum
.write:
	mov	al,cmos_diag
	call	rtc_write
	pop	bx
	pop	ax
	ret

;=========================================================================
; cmos_read_config - read a CMOS configuration byte, or its default if
;	the CMOS is not valid
; Input:
;	AL = CMOS address: cmos_floppy, cmos_equip or cmos_config_a
; Output:
;	AL = the CMOS byte or, with cmos_bad_bat or cmos_bad_sum set in 0Eh,
;	     default_floppy, default_equip or default_config_a
; Note:
;	Jumped to from get_floppy and get_config_a, called from rtc_init
;-------------------------------------------------------------------------
cmos_read_config:
	push	bx
	mov	bl,al			; BL = address
	mov	al,cmos_diag
	call	rtc_read
	test	al,cmos_invalid
	mov	al,bl
	jnz	.default
	call	rtc_read		; the configuration is valid
	jmp	.exit
.default:
	cmp	bl,cmos_floppy
	mov	al,byte [cs:default_floppy]
	je	.exit
	cmp	bl,cmos_equip
	mov	al,byte [cs:default_equip]
	je	.exit
	mov	al,byte [cs:default_config_a]
.exit:
	pop	bx
	ret

;=========================================================================
; cmos_check - compare the CMOS configuration with the hardware, as the
;	IBM PC/AT POST does, record the result in 0Eh and print the errors
; Input:
;	DS = BIOS data area; called from the POST after the memory size
;	(and the EBDA) and the equipment are known, before the extension
;	ROM scan
; Output:
;	CMOS 0Eh = status, post_flags bit post_cmos_err set if a message was
;	printed. Registers preserved
; Notes:
;	The base memory size POST found is kept in 0040:0015h for the BIOS
;	setup (cmos_save_config), which runs after the extension ROMs
;	With a valid CMOS (cmos_validate):
;	- cmos_bad_config if the equipment byte (14h) does not match:
;	  - diskettes: bit 0 and bits 7-6 against the drive types in 10h,
;	    as nvram_save sets them (this BIOS takes the drives from 10h);
;	    with only the second drive set, one drive (00b) is also accepted
;	  - co-processor: bit 1 against detect_fpu
;	  - display (bits 5-4): with a video ROM at C0000h-C7FFFh, not CGA
;	    (10h, 20h); without one, monochrome (30h) exactly when the
;	    display switch selects the monochrome adapter
;	- cmos_mem_size if the base memory in 15h-16h differs from the size
;	  POST found, or the extended memory in 17h-18h from 30h-31h
;	Always: cmos_clk_fail if the RTC stays in its update cycle or holds a
;	time or date that is not valid BCD within limits
;-------------------------------------------------------------------------
cmos_check:
	push	ax
	push	bx
	push	cx
	push	si
	call	cmos_base_size
	mov	word [cmos_post_base],ax ; for the BIOS setup
	mov	al,cmos_diag
	call	rtc_read
	mov	bh,al			; BH = status
	test	bh,cmos_invalid
	jnz	.clock			; nothing to compare

; diskettes: the equipment byte against the drive types

	mov	al,cmos_equip
	call	rtc_read
	mov	bl,al			; BL = CMOS equipment byte
	mov	al,cmos_floppy
	call	rtc_read
	mov	ah,00h			; no drives
	or	al,al
	jz	.floppy_cmp
	mov	ah,equip_floppies	; one drive
	test	al,0Fh
	jz	.floppy_cmp
	test	al,0F0h
	mov	ah,equip_floppies|equip_floppy2	; the second drive is set
	jnz	.floppy_cmp
	mov	al,bl			; only the second drive is set:
	and	al,0C1h			;   one drive (as unemupc98 counts)
	cmp	al,equip_floppies	;   or two (as nvram_save sets it)
	je	.fpu
.floppy_cmp:
	mov	al,bl
	and	al,0C1h			; drives installed, number of drives
	cmp	al,ah
	jne	.config_bad

; co-processor

.fpu:
	mov	al,bl
	xor	al,byte [equipment_list]
	test	al,equip_fpu
	jnz	.config_bad

; display type

	mov	al,bl
	and	al,equip_video		; AL = CMOS display type
	mov	ah,byte [equipment_list]
	and	ah,equip_video		; AH = POST display type (the switch)
	call	cmos_video_rom
	jnc	.display_switch		; no video ROM
	cmp	al,equip_color_40
	je	.config_bad		; CGA with a video ROM
	cmp	al,equip_color_80
	je	.config_bad
	jmp	.memory
.display_switch:
	cmp	al,equip_mono
	je	.display_mono
	cmp	ah,equip_mono
	jne	.memory			; colour, or 00h, with the colour switch
	jmp	.config_bad
.display_mono:
	cmp	ah,equip_mono
	je	.memory			; monochrome with the monochrome switch

.config_bad:
	or	bh,cmos_bad_config

; memory sizes

.memory:
	call	cmos_base_size
	mov	cx,ax			; CX = base memory POST found
	mov	al,cmos_base_hi
	call	cmos_read_word
	and	ah,cmos_base_mask
	cmp	ax,cx
	jne	.memory_bad
	mov	al,cmos_ext_cfg_hi
	call	cmos_read_word
	mov	cx,ax			; CX = configured extended memory
	mov	al,cmos_ext_mem_hi
	call	cmos_read_word		; AX = extended memory size
	cmp	ax,cx
	je	.clock
.memory_bad:
	or	bh,cmos_mem_size

; time and date

.clock:
	call	cmos_clock_check
	jnc	.write
	or	bh,cmos_clk_fail

.write:
	mov	al,cmos_diag
	mov	ah,bh
	call	rtc_write

; messages

	mov	si,cmos_msg_table
.msg_loop:
	test	bh,byte [cs:si]
	jz	.msg_next
	push	si
	mov	si,word [cs:si+1]
	call	print
	pop	si
	or	byte [post_flags],post_cmos_err
.msg_next:
	add	si,3
	cmp	si,cmos_msg_table_end
	jb	.msg_loop
	pop	si
	pop	cx
	pop	bx
	pop	ax
	ret

;=========================================================================
; cmos_error_wait - after a CMOS error message, wait for F1 to continue,
;	or F2 to run the BIOS setup
; Input:
;	DS = BIOS data area; called from boot_os, after the F1 check
; Output:
;	post_flags bit post_setup set if F2 was pressed, bit post_cmos_err
;	cleared
; Notes:
;	- No wait if the setup was already requested (F1 during the POST)
;	- Keys typed before the prompt are discarded, so they cannot answer
;	  it. POST code e_cmos_wait (28h) is shown while waiting
;-------------------------------------------------------------------------
cmos_error_wait:
	test	byte [post_flags],post_cmos_err
	jz	.exit			; no error
	and	byte [post_flags],~post_cmos_err
	test	byte [post_flags],post_setup
	jnz	.exit			; the setup runs next
	push	ax
	push	si
	mov	al,e_cmos_wait
	out	post_reg,al
.flush:
	mov	ah,01h
	int	16h
	jz	.prompt			; no type-ahead left
	mov	ah,00h
	int	16h
	jmp	.flush
.prompt:
	mov	si,msg_cmos_resume
	call	print
.wait:
	mov	ah,00h
	int	16h
	cmp	ah,3Bh			; F1?
	je	.done
	cmp	ah,3Ch			; F2?
	jne	.wait
	or	byte [post_flags],post_setup
.done:
	pop	si
	pop	ax
.exit:
	ret

;=========================================================================
; cmos_save_config - complete the configuration the BIOS setup saves
; Input:
;	AH = equipment byte for CMOS 14h; called from nvram_save, before
;	     the equipment byte and the checksum are written
;	DS = BIOS data area
; Output:
;	AH = equipment byte with the display type 00h if a video ROM is
;	     present; CMOS 15h-16h = base memory POST found (cmos_check kept
;	     it in 0040:0015h), 17h-18h = extended memory size (30h-31h),
;	     0Eh = 00h (CMOS valid)
;	If 0Eh had cmos_bad_bat or cmos_bad_sum set, 11h-13h and 19h-2Ch
;	are cleared, so that the new checksum does not cover stale bytes
;	(upstream clears 10h-20h when the checksum is bad)
;-------------------------------------------------------------------------
cmos_save_config:
	push	bx
	call	cmos_video_rom
	jnc	.clear			; no video ROM
	and	ah,~equip_video		; EGA, VGA (video ROM)
.clear:
	push	ax
	mov	al,cmos_diag
	call	rtc_read
	test	al,cmos_invalid
	jz	.sizes			; the other bytes are valid
	mov	ax,0011h		; start from 11h, load 00h
.clear_loop:
	cmp	al,cmos_equip
	jne	.clear_byte
	mov	al,19h			; skip 14h-18h, written here
.clear_byte:
	call	rtc_write
	inc	al
	cmp	al,cmos_config_a	; up to 2Ch, 2Dh is written by nvram_save
	jb	.clear_loop
.sizes:
	mov	bx,word [cmos_post_base]
	mov	al,cmos_base_lo
	call	cmos_write_word
	mov	al,cmos_ext_mem_hi
	call	cmos_read_word
	mov	bx,ax
	mov	al,cmos_ext_cfg_lo
	call	cmos_write_word
	mov	ax,cmos_diag		; AH = 00h - status: valid
	call	rtc_write
	pop	ax
	pop	bx
	ret

;=========================================================================
; cmos_base_size - base memory size POST found
; Output:
;	AX = 0040:0013h plus the EBDA this BIOS reserved (reserve_ebda)
;-------------------------------------------------------------------------
cmos_base_size:
	mov	ax,word [memory_size]
%ifdef EBDA_SIZE
	test	word [equipment_list],equip_mouse
	jz	.exit			; no EBDA
	add	ax,EBDA_SIZE
.exit:
%endif ; EBDA_SIZE
	ret

;=========================================================================
; cmos_video_rom - look for a video ROM, as the IBM PC/AT POST does
; Output:
;	CF = 1 if a ROM signature (AA55h) is at C0000h-C7FFFh (2 KiB steps)
;-------------------------------------------------------------------------
cmos_video_rom:
	push	cx
	push	es
	mov	cx,0C000h
.scan:
	mov	es,cx
	cmp	word [es:0],0AA55h
	stc
	je	.exit			; found, CF = 1
	add	cx,0080h		; next 2 KiB
	cmp	cx,0C800h
	jb	.scan			; CF = 0 at the end
.exit:
	pop	es
	pop	cx
	ret

;=========================================================================
; cmos_clock_check - check that the RTC holds a valid time and date
; Output:
;	CF = 1 if the RTC stays in its update cycle or a time or date
;	register is not valid BCD within its limits
;-------------------------------------------------------------------------
cmos_clock_check:
	push	ax
	push	cx
	push	si
	mov	cx,cmos_uip_wait
.uip:
	mov	al,cmos_control_a
	call	rtc_read
	test	al,cmos_uip
	loopnz	.uip			; wait for the update to end
	stc
	jnz	.exit			; the RTC stays in the update cycle
	mov	si,cmos_clock_table
.next:
	mov	al,byte [cs:si]
	call	rtc_read
	cmp	al,byte [cs:si+1]
	jb	.exit			; below the minimum, CF = 1
	cmp	byte [cs:si+2],al
	jb	.exit			; above the maximum, CF = 1
	and	al,0Fh
	cmp	al,9
	ja	.bad			; not a BCD digit
	add	si,3
	cmp	si,cmos_clock_table_end
	jb	.next			; CF = 0 at the end
	jmp	.exit
.bad:
	stc
.exit:
	pop	si
	pop	cx
	pop	ax
	ret

;=========================================================================
; cmos_read_word - read a 16-bit value from CMOS, high byte first
; Input:
;	AL = address of the high byte, the low byte is at AL - 1
; Output:
;	AX = value
;-------------------------------------------------------------------------
cmos_read_word:
	push	bx
	mov	bl,al
	call	rtc_read
	mov	ah,al
	mov	al,bl
	dec	al
	call	rtc_read
	pop	bx
	ret

;=========================================================================
; cmos_write_word - write a 16-bit value to CMOS
; Input:
;	AL = address of the low byte, the high byte is at AL + 1
;	BX = value
;-------------------------------------------------------------------------
cmos_write_word:
	mov	ah,bl
	call	rtc_write
	inc	al
	mov	ah,bh
	call	rtc_write
	ret

;-------------------------------------------------------------------------
; RTC registers: address, minimum, maximum (BCD, 24-hour format)
cmos_clock_table:
	db	cmos_seconds, 00h, 59h
	db	cmos_minutes, 00h, 59h
	db	cmos_hours, 00h, 23h
	db	cmos_date, 01h, 31h
	db	cmos_month, 01h, 12h
	db	cmos_year, 00h, 99h
	db	cmos_century, 00h, 99h
cmos_clock_table_end:

;-------------------------------------------------------------------------
; Messages, IBM PC/AT error numbers: status bits, message
cmos_msg_table:
	db	cmos_bad_bat
	dw	msg_cmos_161
	db	cmos_bad_sum | cmos_bad_config
	dw	msg_cmos_162
	db	cmos_clk_fail
	dw	msg_cmos_163
	db	cmos_mem_size
	dw	msg_cmos_164
cmos_msg_table_end:

msg_cmos_161	db	'161-CMOS battery failed', 0Dh, 0Ah, 00h
msg_cmos_162	db	'162-CMOS checksum or configuration error', 0Dh, 0Ah, 00h
msg_cmos_163	db	'163-Time and date not set', 0Dh, 0Ah, 00h
msg_cmos_164	db	'164-Memory size error', 0Dh, 0Ah, 00h
msg_cmos_resume	db	'Press F1 to continue, F2 to run BIOS setup', 0Dh, 0Ah, 00h
