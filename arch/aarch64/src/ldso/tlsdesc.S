/*
 * lib-spfxd — AArch64 TLS descriptor resolvers (R_AARCH64_TLSDESC).
 *
 * A descriptor is two words { resolver, argument }.  Code compiled with
 * the (default) TLSDESC dialect calls the resolver with x0 = &descriptor
 * and expects in x0 the variable's offset from the thread pointer; every
 * other register except x30 (and the condition flags) must be preserved.
 *
 * __tlsdesc_static   modules with static TLS: the argument is the offset.
 * __tlsdesc_dynamic  modules loaded by dlopen: the argument points to
 *                    { module id, offset } for __tls_get_addr, which may
 *                    allocate, so all caller-saved state is spilled.
 */
	.text
	.global __tlsdesc_static
	.hidden __tlsdesc_static
	.type __tlsdesc_static, %function
__tlsdesc_static:
	ldr x0, [x0, #8]
	ret
	.size __tlsdesc_static, . - __tlsdesc_static

	.global __tlsdesc_dynamic
	.hidden __tlsdesc_dynamic
	.type __tlsdesc_dynamic, %function
__tlsdesc_dynamic:
	sub sp, sp, #688
	stp x29, x30, [sp]
	mov x29, sp
	stp x1, x2, [sp, #16]
	stp x3, x4, [sp, #32]
	stp x5, x6, [sp, #48]
	stp x7, x8, [sp, #64]
	stp x9, x10, [sp, #80]
	stp x11, x12, [sp, #96]
	stp x13, x14, [sp, #112]
	stp x15, x16, [sp, #128]
	stp x17, x18, [sp, #144]
	add x1, sp, #160
	stp q0, q1, [x1, #0]
	stp q2, q3, [x1, #32]
	stp q4, q5, [x1, #64]
	stp q6, q7, [x1, #96]
	stp q8, q9, [x1, #128]
	stp q10, q11, [x1, #160]
	stp q12, q13, [x1, #192]
	stp q14, q15, [x1, #224]
	stp q16, q17, [x1, #256]
	stp q18, q19, [x1, #288]
	stp q20, q21, [x1, #320]
	stp q22, q23, [x1, #352]
	stp q24, q25, [x1, #384]
	stp q26, q27, [x1, #416]
	stp q28, q29, [x1, #448]
	stp q30, q31, [x1, #480]
	mrs x2, fpsr
	mrs x3, fpcr
	str x2, [x1, #512]
	str x3, [x1, #520]
	ldr x0, [x0, #8]
	bl __tls_get_addr
	mrs x1, tpidr_el0
	sub x0, x0, x1
	add x1, sp, #160
	ldr x2, [x1, #512]
	ldr x3, [x1, #520]
	msr fpsr, x2
	msr fpcr, x3
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]
	ldp q8, q9, [x1, #128]
	ldp q10, q11, [x1, #160]
	ldp q12, q13, [x1, #192]
	ldp q14, q15, [x1, #224]
	ldp q16, q17, [x1, #256]
	ldp q18, q19, [x1, #288]
	ldp q20, q21, [x1, #320]
	ldp q22, q23, [x1, #352]
	ldp q24, q25, [x1, #384]
	ldp q26, q27, [x1, #416]
	ldp q28, q29, [x1, #448]
	ldp q30, q31, [x1, #480]
	ldp x1, x2, [sp, #16]
	ldp x3, x4, [sp, #32]
	ldp x5, x6, [sp, #48]
	ldp x7, x8, [sp, #64]
	ldp x9, x10, [sp, #80]
	ldp x11, x12, [sp, #96]
	ldp x13, x14, [sp, #112]
	ldp x15, x16, [sp, #128]
	ldp x17, x18, [sp, #144]
	ldp x29, x30, [sp]
	add sp, sp, #688
	ret
	.size __tlsdesc_dynamic, . - __tlsdesc_dynamic

	.section .note.GNU-stack, "", %progbits
