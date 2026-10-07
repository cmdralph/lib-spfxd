/* lib-spfxd test — stack smashing protection: a stack buffer overflow in
 * a function built with -fstack-protector-all is detected at return and
 * terminates the process with SIGABRT. */
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "t.h"

__attribute__((noinline, optimize("stack-protector-all"))) static void smash(const char *s, size_t n)
{
	char buf[8];
	memcpy(buf, s, n);
	__asm__ __volatile__("" : : "r"(buf) : "memory");
}

__attribute__((noinline, optimize("stack-protector-all"))) static int fine(void)
{
	char buf[16];
	memset(buf, 1, sizeof buf);
	__asm__ __volatile__("" : : "r"(buf) : "memory");
	return buf[3];
}

int main(void)
{
	CHECK(fine() == 1, "protected function runs normally");
	pid_t p = fork();
	if (!p) {
		close(2);
		char big[64];
		memset(big, 'A', sizeof big);
		smash(big, sizeof big);
		_exit(0);
	}
	int st;
	waitpid(p, &st, 0);
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT, "overflow detected (status %#x)", st);
	return DONE();
}
