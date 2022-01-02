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

// ----------------------------------------------------------------------------
// 16/7倍に拡大してTMDSエンコード
// r0: Input buffer (word-aligned)
// r1: Output buffer (word-aligned)
// r2: Input size (pixels)
// r3: Left shift amount
decl_func tmds_encode_loop_12bpp_scale16_7
	push {r4, r5, r6, r7, lr}
	lsls r2, #2
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)
	b 2f
.align 2

	// interpolator 1
	//  accum 0: 前の packed ピクセルの右を選択し、Table LSB側に
	//  accum 1: 次の packed ピクセルの左を選択し、Table MSB側に
	// 左シフトで5+4bitの位置に合わせておき、必要量右シフトする
1:
	// s0,1 s2,3 s4,5 s6,7 s8,9 s10,11 s12,13

	ldmia r0!, {r4, r7}						// r4:s0,1 r7:s2,3
	lsls r4, r3
	lsls r7, r3

	str r4, [r2, #ACCUM0_OFFS]				// i0:s0,1
	str r4, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s1
	str r7, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s2

	// s0 -> d0,1
	ldr r4, [r2, #PEEK0_OFFS]				// r4:d0,1
	ldr r4, [r4]	

	// s0,1 -> d2,3
	ldr r5, [r2, #PEEK2_OFFS]
	ldr r5, [r5]							// r5:d2,3

	str r7, [r2, #ACCUM0_OFFS]				// i0:s2,3

	// s1,2 -> d4,5
	ldr r6, [r2, #PEEK2_OFFS + INTERP1]
	ldr r6, [r6, 4]							// r6:d4,5

	str r7, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s3

	// s2,3 -> d6,7
	ldr r7, [r2, #PEEK2_OFFS]
	ldr r7, [r7, 8]							// r7:d6,7

	stmia r1!, {r4, r5, r6, r7}

	//
	ldmia r0!, {r5, r7}						// r5:s4,5 r7:s6,7
	lsls r5, r3
	lsls r7, r3

	str r5, [r2, #ACCUM0_OFFS]				// i0:s4,5
	str r5, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s4

	// s3,4 -> d8,9
	ldr r4, [r2, #PEEK2_OFFS + INTERP1]
	ldr r4, [r4, 12]						// r4:d8,9

	str r5, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s5
	str r7, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s6

	// s4,5 -> d10,11
	ldr r5, [r2, #PEEK2_OFFS]
	ldr r5, [r5, 16]						// r5:d10,11

	// s5,6 -> d12,13
	ldr r6, [r2, #PEEK2_OFFS + INTERP1]
	ldr r6, [r6, 20]						// r6:d12,13

	str r7, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s7

	// s6   -> d14,15
	ldr r7, [r2, #PEEK1_OFFS + INTERP1]
	ldr r7, [r7]							// r6:d14,15

	stmia r1!, {r4, r5, r6, r7}

	/////
	ldmia r0!, {r7}							// r7:s8,9
	lsls r7, r3

	str r7, [r2, #ACCUM0_OFFS]				// i0:s8,9
	str r7, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s8

	// s7     -> d16,17
	ldr r4, [r2, #PEEK0_OFFS + INTERP1]
	ldr r4, [r4]							// r4:d14,15

	// s7,8   -> d18,19
	ldr r5, [r2, #PEEK2_OFFS + INTERP1]
	ldr r5, [r5]							// r5:d18,19

	str r7, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s9

	// s8,9   -> d20,21
	ldr r6, [r2, #PEEK2_OFFS]
	ldr r6, [r6, 4]							// r6:d20,21

	stmia r1!, {r4, r5, r6}

	/////
	ldmia r0!, {r5, r7}						// r5:s10,11 r7:s12,13
	lsls r5, r3
	lsls r7, r3

	str r5, [r2, #ACCUM0_OFFS]				// i0:s10,11
	str r5, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s10

	// s9,10  -> d22,23
	ldr r4, [r2, #PEEK2_OFFS + INTERP1]
	ldr r4, [r4, 8]							// r4:d22,23

	stmia r1!, {r4}

	str r5, [r2, #ACCUM0_OFFS + INTERP1]	// i1a0:s11
	str r7, [r2, #ACCUM1_OFFS + INTERP1]	// i1a1:s12

	// s10,11 -> d24,25
	ldr r4, [r2, #PEEK2_OFFS]
	ldr r4, [r4, 12]						// r6:d20,21

	str r7, [r2, #ACCUM0_OFFS]				// i0:s12,13

	// s11,12 -> d26,27
	ldr r5, [r2, #PEEK2_OFFS + INTERP1]
	ldr r5, [r5, 16]						// r4:d26,27

	// s12,13 -> d28,29
	ldr r6, [r2, #PEEK2_OFFS]
	ldr r6, [r6, 20]						// r6:d28,29

	// s13    -> d30,31 
	ldr r7, [r2, #PEEK1_OFFS]
	ldr r7, [r7]							// r6:d30,31

	stmia r1!, {r4, r5, r6, r7}
2:
	cmp r1, ip
	bne 1b
	pop {r4, r5, r6, r7, pc}
