/* lib-spfxd — <assert.h>.  Deliberately without an include guard: the
 * standard requires NDEBUG to be re-evaluated on every inclusion. */
#include <features.h>

#undef assert
#ifdef NDEBUG
# define assert(x) ((void)0)
#else
# define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__), 0)))
#endif

#if defined(__SPFXD_C11) && !defined(__cplusplus) && !defined(static_assert) && \
    (!defined(__STDC_VERSION__) || __STDC_VERSION__ <= 201710L)
# define static_assert _Static_assert
#endif

#ifndef __SPFXD_ASSERT_DECLARED
#define __SPFXD_ASSERT_DECLARED
__SPFXD_BEGIN_DECLS
__spfxd_noreturn void __assert_fail(const char *, const char *, int, const char *);
__SPFXD_END_DECLS
#endif
