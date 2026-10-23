/*
 * TLS descriptor register-contract check (included by the test libraries).
 *
 * DEFINE_TLSDESC_CHECK(fn, var) defines `void *fn(void)`, which loads
 * known values into every register the descriptor resolver must preserve
 * (the caller-saved general registers and vector registers; the
 * callee-saved ones are preserved by any C code anyway), makes one TLS
 * descriptor call for `var`, verifies all of them, and returns var's
 * address for this thread, or 0 if any register changed.
 *
 * The instruction sequences are the ones the compilers emit for the
 * descriptor dialect, written out so the test does not depend on register
 * allocation.  On other architectures nothing is defined.
 */
#if defined(__x86_64__)
#define TLSDESC_CHECK_SUPPORTED 1
#define DEFINE_TLSDESC_CHECK(fn, var) __asm__ ( \
	".pushsection .text\n.globl " #fn "\n.type " #fn ",@function\n" #fn ":\n" \
	"movq $4097, %rcx\n" \
	"movq $4098, %rdx\n" \
	"movq $4099, %rsi\n" \
	"movq $4100, %rdi\n" \
	"movq $4101, %r8\n" \
	"movq $4102, %r9\n" \
	"movq $4103, %r10\n" \
	"movq $4104, %r11\n" \
	"movl $8192, %eax\n movd %eax, %xmm0\n" \
	"movl $8193, %eax\n movd %eax, %xmm1\n" \
	"movl $8194, %eax\n movd %eax, %xmm2\n" \
	"movl $8195, %eax\n movd %eax, %xmm3\n" \
	"movl $8196, %eax\n movd %eax, %xmm4\n" \
	"movl $8197, %eax\n movd %eax, %xmm5\n" \
	"movl $8198, %eax\n movd %eax, %xmm6\n" \
	"movl $8199, %eax\n movd %eax, %xmm7\n" \
	"movl $8200, %eax\n movd %eax, %xmm8\n" \
	"movl $8201, %eax\n movd %eax, %xmm9\n" \
	"movl $8202, %eax\n movd %eax, %xmm10\n" \
	"movl $8203, %eax\n movd %eax, %xmm11\n" \
	"movl $8204, %eax\n movd %eax, %xmm12\n" \
	"movl $8205, %eax\n movd %eax, %xmm13\n" \
	"movl $8206, %eax\n movd %eax, %xmm14\n" \
	"movl $8207, %eax\n movd %eax, %xmm15\n" \
	"leaq " #var "@TLSDESC(%rip), %rax\n" \
	"call *" #var "@TLSCALL(%rax)\n" \
	"cmpq $4097, %rcx\n jne 9f\n" \
	"cmpq $4098, %rdx\n jne 9f\n" \
	"cmpq $4099, %rsi\n jne 9f\n" \
	"cmpq $4100, %rdi\n jne 9f\n" \
	"cmpq $4101, %r8\n jne 9f\n" \
	"cmpq $4102, %r9\n jne 9f\n" \
	"cmpq $4103, %r10\n jne 9f\n" \
	"cmpq $4104, %r11\n jne 9f\n" \
	"movd %xmm0, %ecx\n cmpl $8192, %ecx\n jne 9f\n" \
	"movd %xmm1, %ecx\n cmpl $8193, %ecx\n jne 9f\n" \
	"movd %xmm2, %ecx\n cmpl $8194, %ecx\n jne 9f\n" \
	"movd %xmm3, %ecx\n cmpl $8195, %ecx\n jne 9f\n" \
	"movd %xmm4, %ecx\n cmpl $8196, %ecx\n jne 9f\n" \
	"movd %xmm5, %ecx\n cmpl $8197, %ecx\n jne 9f\n" \
	"movd %xmm6, %ecx\n cmpl $8198, %ecx\n jne 9f\n" \
	"movd %xmm7, %ecx\n cmpl $8199, %ecx\n jne 9f\n" \
	"movd %xmm8, %ecx\n cmpl $8200, %ecx\n jne 9f\n" \
	"movd %xmm9, %ecx\n cmpl $8201, %ecx\n jne 9f\n" \
	"movd %xmm10, %ecx\n cmpl $8202, %ecx\n jne 9f\n" \
	"movd %xmm11, %ecx\n cmpl $8203, %ecx\n jne 9f\n" \
	"movd %xmm12, %ecx\n cmpl $8204, %ecx\n jne 9f\n" \
	"movd %xmm13, %ecx\n cmpl $8205, %ecx\n jne 9f\n" \
	"movd %xmm14, %ecx\n cmpl $8206, %ecx\n jne 9f\n" \
	"movd %xmm15, %ecx\n cmpl $8207, %ecx\n jne 9f\n" \
	"addq %fs:0, %rax\n ret\n" \
	"9: xorl %eax, %eax\n ret\n" \
	".size " #fn ", . - " #fn "\n.popsection\n")
#elif defined(__aarch64__)
#define TLSDESC_CHECK_SUPPORTED 1
#define DEFINE_TLSDESC_CHECK(fn, var) __asm__ ( \
	".pushsection .text\n.globl " #fn "\n.type " #fn ",%function\n" #fn ":\n" \
	"stp x29, x30, [sp, #-16]!\n mov x29, sp\n" \
	"mov x2, #258\n" \
	"mov x3, #259\n" \
	"mov x4, #260\n" \
	"mov x5, #261\n" \
	"mov x6, #262\n" \
	"mov x7, #263\n" \
	"mov x8, #264\n" \
	"mov x9, #265\n" \
	"mov x10, #266\n" \
	"mov x11, #267\n" \
	"mov x12, #268\n" \
	"mov x13, #269\n" \
	"mov x14, #270\n" \
	"mov x15, #271\n" \
	"mov x16, #272\n" \
	"mov x17, #273\n" \
	"mov x18, #274\n" \
	"mov w0, #768\n fmov s0, w0\n" \
	"mov w0, #769\n fmov s1, w0\n" \
	"mov w0, #770\n fmov s2, w0\n" \
	"mov w0, #771\n fmov s3, w0\n" \
	"mov w0, #772\n fmov s4, w0\n" \
	"mov w0, #773\n fmov s5, w0\n" \
	"mov w0, #774\n fmov s6, w0\n" \
	"mov w0, #775\n fmov s7, w0\n" \
	"mov w0, #784\n fmov s16, w0\n" \
	"mov w0, #785\n fmov s17, w0\n" \
	"mov w0, #786\n fmov s18, w0\n" \
	"mov w0, #787\n fmov s19, w0\n" \
	"mov w0, #788\n fmov s20, w0\n" \
	"mov w0, #789\n fmov s21, w0\n" \
	"mov w0, #790\n fmov s22, w0\n" \
	"mov w0, #791\n fmov s23, w0\n" \
	"mov w0, #792\n fmov s24, w0\n" \
	"mov w0, #793\n fmov s25, w0\n" \
	"mov w0, #794\n fmov s26, w0\n" \
	"mov w0, #795\n fmov s27, w0\n" \
	"mov w0, #796\n fmov s28, w0\n" \
	"mov w0, #797\n fmov s29, w0\n" \
	"mov w0, #798\n fmov s30, w0\n" \
	"mov w0, #799\n fmov s31, w0\n" \
	"adrp x0, :tlsdesc:" #var "\n" \
	"ldr x1, [x0, #:tlsdesc_lo12:" #var "]\n" \
	"add x0, x0, #:tlsdesc_lo12:" #var "\n" \
	".tlsdesccall " #var "\n" \
	"blr x1\n" \
	"cmp x2, #258\n b.ne 9f\n" \
	"cmp x3, #259\n b.ne 9f\n" \
	"cmp x4, #260\n b.ne 9f\n" \
	"cmp x5, #261\n b.ne 9f\n" \
	"cmp x6, #262\n b.ne 9f\n" \
	"cmp x7, #263\n b.ne 9f\n" \
	"cmp x8, #264\n b.ne 9f\n" \
	"cmp x9, #265\n b.ne 9f\n" \
	"cmp x10, #266\n b.ne 9f\n" \
	"cmp x11, #267\n b.ne 9f\n" \
	"cmp x12, #268\n b.ne 9f\n" \
	"cmp x13, #269\n b.ne 9f\n" \
	"cmp x14, #270\n b.ne 9f\n" \
	"cmp x15, #271\n b.ne 9f\n" \
	"cmp x16, #272\n b.ne 9f\n" \
	"cmp x17, #273\n b.ne 9f\n" \
	"cmp x18, #274\n b.ne 9f\n" \
	"fmov w9, s0\n cmp w9, #768\n b.ne 9f\n" \
	"fmov w9, s1\n cmp w9, #769\n b.ne 9f\n" \
	"fmov w9, s2\n cmp w9, #770\n b.ne 9f\n" \
	"fmov w9, s3\n cmp w9, #771\n b.ne 9f\n" \
	"fmov w9, s4\n cmp w9, #772\n b.ne 9f\n" \
	"fmov w9, s5\n cmp w9, #773\n b.ne 9f\n" \
	"fmov w9, s6\n cmp w9, #774\n b.ne 9f\n" \
	"fmov w9, s7\n cmp w9, #775\n b.ne 9f\n" \
	"fmov w9, s16\n cmp w9, #784\n b.ne 9f\n" \
	"fmov w9, s17\n cmp w9, #785\n b.ne 9f\n" \
	"fmov w9, s18\n cmp w9, #786\n b.ne 9f\n" \
	"fmov w9, s19\n cmp w9, #787\n b.ne 9f\n" \
	"fmov w9, s20\n cmp w9, #788\n b.ne 9f\n" \
	"fmov w9, s21\n cmp w9, #789\n b.ne 9f\n" \
	"fmov w9, s22\n cmp w9, #790\n b.ne 9f\n" \
	"fmov w9, s23\n cmp w9, #791\n b.ne 9f\n" \
	"fmov w9, s24\n cmp w9, #792\n b.ne 9f\n" \
	"fmov w9, s25\n cmp w9, #793\n b.ne 9f\n" \
	"fmov w9, s26\n cmp w9, #794\n b.ne 9f\n" \
	"fmov w9, s27\n cmp w9, #795\n b.ne 9f\n" \
	"fmov w9, s28\n cmp w9, #796\n b.ne 9f\n" \
	"fmov w9, s29\n cmp w9, #797\n b.ne 9f\n" \
	"fmov w9, s30\n cmp w9, #798\n b.ne 9f\n" \
	"fmov w9, s31\n cmp w9, #799\n b.ne 9f\n" \
	"mrs x9, tpidr_el0\n add x0, x0, x9\n" \
	"ldp x29, x30, [sp], #16\n ret\n" \
	"9: mov x0, #0\n ldp x29, x30, [sp], #16\n ret\n" \
	".size " #fn ", . - " #fn "\n.popsection\n")
#else
#define DEFINE_TLSDESC_CHECK(fn, var)
#endif
