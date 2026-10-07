/* lib-spfxd — BSD err/warn diagnostics. */
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *program_invocation_short_name;

void vwarn(const char *fmt, va_list ap)
{
	int e = errno;
	flockfile(stderr);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fputs(": ", stderr);
	}
	fprintf(stderr, "%s\n", strerror(e));
	funlockfile(stderr);
}

void vwarnx(const char *fmt, va_list ap)
{
	flockfile(stderr);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	if (fmt) vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	funlockfile(stderr);
}

_Noreturn void verr(int status, const char *fmt, va_list ap)
{
	vwarn(fmt, ap);
	exit(status);
}

_Noreturn void verrx(int status, const char *fmt, va_list ap)
{
	vwarnx(fmt, ap);
	exit(status);
}

void warn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarn(fmt, ap); va_end(ap); }
void warnx(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarnx(fmt, ap); va_end(ap); }
_Noreturn void err(int s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); verr(s, fmt, ap); }
_Noreturn void errx(int s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); verrx(s, fmt, ap); }
