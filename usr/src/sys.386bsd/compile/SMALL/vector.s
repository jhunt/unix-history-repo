/*
 * AT/386
 * Interrupt vector routines
 * Generated by config program
 */ 

#include	"i386/isa/isa.h"
#include	"i386/isa/icu.h"

#define	VEC(name)	.align 4; .globl _V/**/name; _V/**/name:

	.globl	_hardclock
VEC(clk)
	INTR(0, _highmask, 0)
	call	_hardclock 
	INTREXIT1


	.globl _wdintr, _wd0mask
	.data
_wd0mask:	.long 0
	.text
VEC(wd0)
	INTR(0, _biomask, 1)
	call	_wdintr
	INTREXIT2


	.globl _fdintr, _fd0mask
	.data
_fd0mask:	.long 0
	.text
VEC(fd0)
	INTR(0, _biomask, 2)
	call	_fdintr
	INTREXIT1


	.globl _pcrint, _pc0mask
	.data
_pc0mask:	.long 0
	.text
VEC(pc0)
	INTR(0, _ttymask, 3)
	call	_pcrint
	INTREXIT1


	.globl _npxintr, _npx0mask
	.data
_npx0mask:	.long 0
	.text
VEC(npx0)
	INTR(0, _npx0mask, 4)
	call	_npxintr
	INTREXIT2


	.globl _comintr, _com0mask
	.data
_com0mask:	.long 0
	.text
VEC(com0)
	INTR(0, _ttymask, 5)
	call	_comintr
	INTREXIT1


	.globl _weintr, _we0mask
	.data
_we0mask:	.long 0
	.text
VEC(we0)
	INTR(0, _netmask, 6)
	call	_weintr
	INTREXIT1


