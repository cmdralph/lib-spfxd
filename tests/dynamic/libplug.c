/* test plugin: loaded with dlopen; dynamic TLS and a symbol from the program */
#include <string.h>
__thread long plug_tls = 1234;
__thread char plug_buf[256];
extern int prog_exported(int);
int plug_ctor;
__attribute__((constructor)) static void plug_init(void) { plug_ctor = 1; }
long plug_tls_get(void) { return plug_tls; }
void plug_tls_set(long v) { plug_tls = v; strcpy(plug_buf, "x"); }
int plug_call_prog(int x) { return prog_exported(x) * 2; }
int plug_strlen(const char *s) { return (int)strlen(s); }
