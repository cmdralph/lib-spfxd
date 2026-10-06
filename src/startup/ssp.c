/*
 * lib-spfxd — stack-smashing protector support.
 *
 * The canary comes from the kernel-supplied AT_RANDOM bytes.  Its lowest
 * byte is forced to zero so that string-copy overflows (which stop at a NUL)
 * cannot reproduce it.  The second 8 random bytes seed the library's
 * pointer-hardening secret (used by the allocator's free lists).
 */
#include <string.h>
#include <unistd.h>
#include "libc.h"
#include "pthread_impl.h"

hidden void __init_ssp(void *entropy)
{
	uintptr_t canary, secret;
	if (entropy) {
		memcpy(&canary, entropy, sizeof canary);
		memcpy(&secret, (char *)entropy + 8, sizeof secret);
	} else {
		canary = (uintptr_t)&canary * 1103515245UL ^ 0x5bd1e9955bd1e995UL;
		secret = canary * 0x9e3779b97f4a7c15UL;
	}
	canary &= ~(uintptr_t)0xff;
	__self()->canary = canary;
	__libc.secret = secret | 1;
}

void __stack_chk_fail(void)
{
	static const char msg[] = "*** stack smashing detected ***: terminated\n";
	__syscall(SYS_write, 2, msg, sizeof msg - 1);
	a_crash();
}

hidden void __stack_chk_fail_local(void)
{
	__stack_chk_fail();
}
