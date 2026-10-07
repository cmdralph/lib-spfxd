/*
 * lib-spfxd — <ctype.h> functions.  The header provides equivalent macros;
 * these out-of-line definitions exist for function pointers, C++ and code
 * that #undefs the macros.  Arguments outside [-128, 255] (other than EOF)
 * are undefined behaviour per ISO C; they are clamped to "no class" here
 * rather than indexing out of bounds.
 */
#include <ctype.h>

#undef isalnum
#undef isalpha
#undef isblank
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit

static __inline int has(int c, unsigned m)
{
	return (unsigned)(c + 128) < 384u && (__spfxd_ctype_tab[c + 128] & m) != 0;
}

int isalnum(int c)  { return has(c, __SPFXD_CT_ALNUM); }
int isalpha(int c)  { return has(c, __SPFXD_CT_ALPHA); }
int isblank(int c)  { return c == ' ' || c == '\t'; }
int iscntrl(int c)  { return has(c, __SPFXD_CT_CNTRL); }
int isdigit(int c)  { return (unsigned)c - '0' < 10; }
int isgraph(int c)  { return (unsigned)c - 0x21 < 0x5e; }
int islower(int c)  { return (unsigned)c - 'a' < 26; }
int isprint(int c)  { return (unsigned)c - 0x20 < 0x5f; }
int ispunct(int c)  { return has(c, __SPFXD_CT_PUNCT); }
int isspace(int c)  { return c == ' ' || (unsigned)c - '\t' < 5; }
int isupper(int c)  { return (unsigned)c - 'A' < 26; }
int isxdigit(int c) { return has(c, __SPFXD_CT_XDIGIT); }
int tolower(int c)  { return (unsigned)c - 'A' < 26 ? c | 32 : c; }
int toupper(int c)  { return (unsigned)c - 'a' < 26 ? c & 0x5f : c; }
int isascii(int c)  { return !(c & ~0x7f); }
int toascii(int c)  { return c & 0x7f; }

int isalnum_l(int c, locale_t l)  { return isalnum(c); }
int isalpha_l(int c, locale_t l)  { return isalpha(c); }
int isblank_l(int c, locale_t l)  { return isblank(c); }
int iscntrl_l(int c, locale_t l)  { return iscntrl(c); }
int isdigit_l(int c, locale_t l)  { return isdigit(c); }
int isgraph_l(int c, locale_t l)  { return isgraph(c); }
int islower_l(int c, locale_t l)  { return islower(c); }
int isprint_l(int c, locale_t l)  { return isprint(c); }
int ispunct_l(int c, locale_t l)  { return ispunct(c); }
int isspace_l(int c, locale_t l)  { return isspace(c); }
int isupper_l(int c, locale_t l)  { return isupper(c); }
int isxdigit_l(int c, locale_t l) { return isxdigit(c); }
int tolower_l(int c, locale_t l)  { return tolower(c); }
int toupper_l(int c, locale_t l)  { return toupper(c); }
