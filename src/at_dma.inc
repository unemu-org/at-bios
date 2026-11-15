;=========================================================================
; at_dma.inc - IBM PC/AT compatible initialization of the second DMA
;	controller (8237, channels 4-7), which cascades the first one
;-------------------------------------------------------------------------
;
; Compiles with NASM 2.13.02, might work with other versions
;
; Copyright (C) 2026 unemu contributors.
; Part of the unemu fork of Sergey Kiselev's 8088_bios.
; The DMAC initialization sequence follows the one for the first DMAC in
; bios.asm, by Sergey Kiselev.
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
; Second DMAC registers: the AT wires address lines A1-A4 to the 8237
; register select, so register N is at port 0C0h + N * 2

dmac2_cmd_reg		equ	0D0h	; DMAC 2 command register (W)
dmac2_mask_reg		equ	0D4h	; DMAC 2 single mask bit register (W)
dmac2_mode_reg		equ	0D6h	; DMAC 2 mode register (W)
dmac2_clear_reg		equ	0DAh	; DMAC 2 master clear register (W)

;-------------------------------------------------------------------------
; dmac2_init - initialize the second DMAC as the IBM PC/AT POST does:
;	channel 4 in cascade mode and unmasked, so that channels 0-3 of
;	the first DMAC reach the bus through it
; Input:
;	none, entered from the POST by a jump, without a valid stack
; Output:
;	channel 4 cascade mode, unmasked; channels 5-7 single mode, verify,
;	masked, their page registers cleared; command register cleared
;	returns to dmac2_init_done
;-------------------------------------------------------------------------
dmac2_init:
	out	dmac2_clear_reg,al	; DMA Master Clear register - reset DMA
	mov	al,0C0h			; cascade mode, channel 4
	out	dmac2_mode_reg,al	; DMA Mode register
	mov	al,41h			; single mode, verify, channel 5
	out	dmac2_mode_reg,al	; DMA Mode register
	mov	al,42h			; single mode, verify, channel 6
	out	dmac2_mode_reg,al	; DMA Mode register
	mov	al,43h			; single mode, verify, channel 7
	out	dmac2_mode_reg,al	; DMA Mode register
	mov	al,0			; DMA Command register bits:
					; same as for the first DMAC
	out	dmac2_cmd_reg,al	; DMA Command register
	out	89h,al			; DMA Page, channel 6
	out	8Ah,al			; DMA Page, channel 7
	out	8Bh,al			; DMA Page, channel 5
	out	dmac2_mask_reg,al	; DMA Single Mask register:
					; unmask channel 4 (cascade)
	jmp	dmac2_init_done
