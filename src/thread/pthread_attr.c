/* lib-spfxd — pthread_attr_* */
#include <errno.h>
#include <string.h>
#include "pthread_impl.h"

int pthread_attr_init(pthread_attr_t *a)
{
	memset(a, 0, sizeof *a);
	a->__guardsize = __libc.default_guard;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *a) { return 0; }

int pthread_attr_getguardsize(const pthread_attr_t *restrict a, size_t *restrict s)
{
	*s = a->__guardsize;
	return 0;
}

int pthread_attr_setguardsize(pthread_attr_t *a, size_t s)
{
	if (s > SIZE_MAX / 8) return EINVAL;
	a->__guardsize = s;
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *restrict a, size_t *restrict s)
{
	*s = a->__stacksize ? a->__stacksize : __libc.default_stack;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *a, size_t s)
{
	if (s < PTHREAD_STACK_MIN || s > SIZE_MAX / 4) return EINVAL;
	a->__stackaddr = 0;
	a->__stacksize = s;
	return 0;
}

int pthread_attr_getstack(const pthread_attr_t *restrict a, void **restrict addr, size_t *restrict size)
{
	if (!a->__stackaddr) return EINVAL;
	*size = a->__stacksize;
	*addr = a->__stackaddr;
	return 0;
}

int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size)
{
	if (size < PTHREAD_STACK_MIN || size > SIZE_MAX / 4) return EINVAL;
	a->__stackaddr = addr;
	a->__stacksize = size;
	return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *a, int *s)
{
	*s = a->__detach;
	return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->__detach = s;
	return 0;
}

int pthread_attr_getscope(const pthread_attr_t *restrict a, int *restrict s)
{
	*s = PTHREAD_SCOPE_SYSTEM;
	return 0;
}

int pthread_attr_setscope(pthread_attr_t *a, int s)
{
	if (s == PTHREAD_SCOPE_SYSTEM) return 0;
	return s == PTHREAD_SCOPE_PROCESS ? ENOTSUP : EINVAL;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *restrict a, int *restrict p)
{
	*p = a->__sched_policy;
	return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *a, int p)
{
	a->__sched_policy = p;
	return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *restrict a, struct sched_param *restrict p)
{
	memset(p, 0, sizeof *p);
	p->sched_priority = a->__sched_prio;
	return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *restrict a, const struct sched_param *restrict p)
{
	a->__sched_prio = p->sched_priority;
	return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *restrict a, int *restrict i)
{
	*i = a->__inherit;
	return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t *a, int i)
{
	if ((unsigned)i > 1) return EINVAL;
	a->__inherit = i;
	return 0;
}

int pthread_getattr_np(pthread_t th, pthread_attr_t *a)
{
	struct pthread *t = (struct pthread *)th;
	pthread_attr_init(a);
	a->__detach = t->detach_state == DT_DETACHED;
	a->__guardsize = t->guard_size;
	if (t->stack) {
		a->__stackaddr = (char *)t->stack - t->stack_size;
		a->__stacksize = t->stack_size;
	} else {
		/* initial thread: report the current stack rlimit region */
		size_t l = __libc.default_stack;
		a->__stacksize = l;
		a->__stackaddr = (char *)__builtin_frame_address(0) - l / 2;
	}
	return 0;
}
