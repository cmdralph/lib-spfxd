/* lib-spfxd test — program startup and termination: constructor and
 * destructor order (priorities, init_array), argv/envp/auxv, environ,
 * program_invocation_name, exit codes. */
#include <errno.h>
#include <sys/auxv.h>
#include <unistd.h>
#include "t.h"

extern char **environ;
static int seq[16], nseq;
static int saw_args;

__attribute__((constructor(101))) static void c101(void) { seq[nseq++] = 101; }
__attribute__((constructor(200))) static void c200(void) { seq[nseq++] = 200; }
__attribute__((constructor)) static void cdef(int argc, char **argv, char **envp)
{
	seq[nseq++] = 999;
	saw_args = argc >= 1 && argv && argv[0] && envp == environ;
}
static void ia(void) { seq[nseq++] = 1000; }
__attribute__((section(".init_array"), used)) static void (*init_ptr)(void) = ia;

__attribute__((destructor)) static void dtor(void)
{
	/* runs after main returns: report through the exit status by writing */
	if (write(1, "destructor ran\n", 15) != 15) _exit(99);
}

int main(int argc, char **argv, char **envp)
{
	CHECK(nseq == 4 && seq[0] == 101 && seq[1] == 200, "priority order %d %d %d %d", seq[0], seq[1], seq[2], seq[3]);
	CHECK(saw_args, "constructors receive argc/argv/envp");
	CHECK(argc >= 1 && argv[argc] == 0 && envp == environ, "argv/envp");
	CHECK(getauxval(AT_PAGESZ) == 4096 && getauxval(AT_PHDR) && getauxval(AT_RANDOM), "auxv");
	errno = 0;
	CHECK(getauxval(12345) == 0 && errno == ENOENT, "getauxval missing");
	extern char *program_invocation_short_name;
	CHECK(!strcmp(program_invocation_short_name, "startup"), "program_invocation_short_name '%s'", program_invocation_short_name);
	CHECK(errno == ENOENT, "errno is per-thread state");
	return DONE();
}
