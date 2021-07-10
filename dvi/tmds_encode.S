#include "hardware/regs/addressmap.h"
#include "hardware/regs/sio.h"

// Offsets suitable for ldr/str (must be <= 0x7c):
#define ACCUM0_OFFS     (SIO_INTERP0_ACCUM0_OFFSET     - SIO_INTERP0_ACCUM0_OFFSET)
#define ACCUM1_OFFS     (SIO_INTERP0_ACCUM1_OFFSET     - SIO_INTERP0_ACCUM0_OFFSET)
#define ACCUM1_ADD_OFFS (SIO_INTERP0_ACCUM1_ADD_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define PEEK0_OFFS      (SIO_INTERP0_PEEK_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define PEEK1_OFFS      (SIO_INTERP0_PEEK_LANE1_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define PEEK2_OFFS      (SIO_INTERP0_PEEK_FULL_OFFSET  - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1         (SIO_INTERP1_ACCUM0_OFFSET     - SIO_INTERP0_ACCUM0_OFFSET)
// Note the entirety of INTERP0 and INTERP1 fits inside this 5-bit
// word-addressed space... almost as though it were intentional! :)

.syntax unified
.cpu cortex-m0plus
.thumb

.macro decl_func_x name
.section .scratch_x.\name, "ax"
.global \name
.type \name,%function
.thumb_func
\name:
.endm

.macro decl_func_y name
.section .scratch_y.\name, "ax"
.global \name
.type \name,%function
.thumb_func
\name:
.endm

#define decl_func decl_func_x

// ----------------------------------------------------------------------------
// Pixel-doubling encoders for RGB

// r0: Input buffer (word-aligned)
// r1: Output buffer (word-aligned)
// r2: Input size (pixels)

.macro do_channel_16bpp r_ibase r_inout0 r_out1
	str \r_inout0, [\r_ibase, #ACCUM0_OFFS]
	ldr \r_inout0, [\r_ibase, #PEEK0_OFFS]
	ldr \r_inout0, [\r_inout0]
	ldr \r_out1, [\r_ibase, #PEEK1_OFFS]
	ldr \r_out1, [\r_out1]
.endm

decl_func tmds_encode_loop_16bpp
	push {r4, r5, r6, r7, lr}
	lsls r2, #2
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)
	b 2f
.align 2
1:
.rept 1
	ldmia r0!, {r4, r6}
	do_channel_16bpp r2, r4, r5
	do_channel_16bpp r2, r6, r7
	stmia r1!, {r4, r5, r6, r7}
.endr
2:
	cmp r1, ip
	bne 1b
	pop {r4, r5, r6, r7, pc}

// Same as above, but scale data to make up for lack of left shift
// in interpolator (costs 1 cycle per 2 pixels)
//
// r0: Input buffer (word-aligned)
// r1: Output buffer (word-aligned)
// r2: Input size (pixels)
// r3: Left shift amount

decl_func tmds_encode_loop_16bpp_leftshift
	push {r4, r5, r6, r7, lr}
	lsls r2, #2
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)
	b 2f
.align 2
1:
.rept 1
	ldmia r0!, {r4, r6}
	lsls r4, r3
	do_channel_16bpp r2, r4, r5
	lsls r6, r3
	do_channel_16bpp r2, r6, r7
	stmia r1!, {r4, r5, r6, r7}
.endr
2:
	cmp r1, ip
	bne 1b
	pop {r4, r5, r6, r7, pc}
