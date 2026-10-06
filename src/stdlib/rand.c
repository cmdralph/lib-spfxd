/*
 * lib-spfxd — pseudo-random generators (NOT cryptographically secure; use
 * arc4random/getrandom/getentropy for that).
 *
 *   rand/srand    64-bit linear congruential state, output scrambled with
 *                 a xorshift-multiply step (PCG-style); RAND_MAX = 2^31-1
 *   rand_r        POSIX re-entrant variant with only 32 bits of state
 *   drand48 family  exactly the 48-bit LCG POSIX specifies
 *                 (a = 0x5DEECE66D, c = 0xB) so sequences are portable
 */
#include <stdlib.h>
#include <stdint.h>
#include "lock.h"

static uint64_t rand_state = 1;
static volatile int rand_lock;

static __inline uint32_t scramble64(uint64_t x)
{
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	return (uint32_t)(x >> 33);
}

void srand(unsigned seed)
{
	__lock(&rand_lock);
	rand_state = seed - 1ULL;
	__unlock(&rand_lock);
}

int rand(void)
{
	__lock(&rand_lock);
	rand_state = rand_state * 6364136223846793005ULL + 1442695040888963407ULL;
	uint64_t s = rand_state;
	__unlock(&rand_lock);
	return (int)(scramble64(s) & 0x7fffffff);
}

int rand_r(unsigned *seed)
{
	/* 32-bit LCG followed by an output mixing step */
	unsigned x = *seed = *seed * 1103515245u + 12345u;
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return (int)(x & 0x7fffffff);
}

/* ---- the drand48 family ---- */

static unsigned short seed48_state[7] = { 0x330e, 0xabcd, 0x1234, 0xe66d, 0xdeec, 0x5, 0xb };

static uint64_t lcg48(unsigned short x[3], const unsigned short *p)
{
	uint64_t a = (uint64_t)p[0] | (uint64_t)p[1] << 16 | (uint64_t)p[2] << 32;
	uint64_t v = (uint64_t)x[0] | (uint64_t)x[1] << 16 | (uint64_t)x[2] << 32;
	v = (v * a + p[3]) & 0xffffffffffffULL;
	x[0] = (unsigned short)v;
	x[1] = (unsigned short)(v >> 16);
	x[2] = (unsigned short)(v >> 32);
	return v;
}

static uint64_t next48(unsigned short x[3])
{
	return lcg48(x, seed48_state + 3);
}

double erand48(unsigned short x[3])
{
	uint64_t v = next48(x);
	union { uint64_t i; double d; } u = { 0x3ff0000000000000ULL | v << 4 };
	return u.d - 1.0;
}

double drand48(void) { return erand48(seed48_state); }
long nrand48(unsigned short x[3]) { return (long)(next48(x) >> 17); }
long lrand48(void) { return nrand48(seed48_state); }
long jrand48(unsigned short x[3]) { return (long)(int32_t)(next48(x) >> 16); }
long mrand48(void) { return jrand48(seed48_state); }

void srand48(long seed)
{
	seed48_state[0] = 0x330e;
	seed48_state[1] = (unsigned short)seed;
	seed48_state[2] = (unsigned short)(seed >> 16);
	seed48_state[3] = 0xe66d;
	seed48_state[4] = 0xdeec;
	seed48_state[5] = 0x5;
	seed48_state[6] = 0xb;
}

unsigned short *seed48(unsigned short s[3])
{
	static unsigned short prev[3];
	prev[0] = seed48_state[0];
	prev[1] = seed48_state[1];
	prev[2] = seed48_state[2];
	seed48_state[0] = s[0];
	seed48_state[1] = s[1];
	seed48_state[2] = s[2];
	seed48_state[3] = 0xe66d;
	seed48_state[4] = 0xdeec;
	seed48_state[5] = 0x5;
	seed48_state[6] = 0xb;
	return prev;
}

void lcong48(unsigned short p[7])
{
	for (int i = 0; i < 7; i++) seed48_state[i] = p[i];
}
