/*
 * lib-spfxd — random / srandom / initstate / setstate.
 *
 * An additive lagged-Fibonacci generator x[n] = x[n-r] + x[n-s] (mod 2^32)
 * over a caller-supplied state array, as POSIX describes: state sizes of
 * 8, 32, 64, 128 and 256 bytes select lags (0 = plain LCG), (7,3), (15,1),
 * (31,3) and (63,1).  The first word of the state buffer records the
 * generator type and position so setstate can resume it.
 */
#include <stdlib.h>
#include <stdint.h>
#include "lock.h"

struct lfg_type { int words, r, s; };
static const struct lfg_type types[5] = {
	{ 0, 0, 0 }, { 7, 7, 3 }, { 15, 15, 1 }, { 31, 31, 3 }, { 63, 63, 1 },
};

static uint32_t default_state[32] = { 3 << 24 };
static uint32_t *state = default_state;   /* state[0] = type<<24 | front<<12 | rear */
static uint32_t *tbl = default_state + 1;
static int type = 3, front_i = 3, rear_i = 0;
static volatile int lock;
static int seeded;

static void save_meta(void)
{
	state[0] = (uint32_t)type << 24 | (uint32_t)front_i << 12 | (uint32_t)rear_i;
}

static uint32_t lcg31(uint32_t x)
{
	return (x * 1103515245u + 12345u) & 0x7fffffff;
}

static void seed_locked(unsigned seed)
{
	const struct lfg_type *t = &types[type];
	seeded = 1;
	if (!t->words) {
		tbl[0] = seed;
		save_meta();
		return;
	}
	uint64_t x = seed ? seed : 1;
	for (int i = 0; i < t->words; i++) {
		/* splitmix-style fill avoids weak low-entropy initial states */
		x += 0x9e3779b97f4a7c15ULL;
		uint64_t z = x;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		tbl[i] = (uint32_t)(z ^ (z >> 31));
	}
	front_i = t->s;
	rear_i = 0;
	/* discard the start-up transient */
	for (int i = 0; i < 10 * t->words; i++) {
		tbl[front_i] += tbl[rear_i];
		if (++front_i >= t->words) front_i = 0;
		if (++rear_i >= t->words) rear_i = 0;
	}
	save_meta();
}

void srandom(unsigned seed)
{
	__lock(&lock);
	seed_locked(seed);
	__unlock(&lock);
}

long random(void)
{
	long r;
	__lock(&lock);
	if (!seeded) seed_locked(1);   /* as if srandom(1) */
	const struct lfg_type *t = &types[type];
	if (!t->words) {
		r = (long)(tbl[0] = lcg31(tbl[0]));
	} else {
		uint32_t v = tbl[front_i] += tbl[rear_i];
		r = (long)(v >> 1);
		if (++front_i >= t->words) front_i = 0;
		if (++rear_i >= t->words) rear_i = 0;
	}
	save_meta();
	__unlock(&lock);
	return r;
}

char *initstate(unsigned seed, char *buf, size_t size)
{
	if (size < 8) return 0;
	__lock(&lock);
	char *old = (char *)state;
	save_meta();
	type = size < 32 ? 0 : size < 64 ? 1 : size < 128 ? 2 : size < 256 ? 3 : 4;
	state = (uint32_t *)(void *)buf;
	tbl = state + 1;
	seed_locked(seed);
	__unlock(&lock);
	return old;
}

char *setstate(char *buf)
{
	__lock(&lock);
	char *old = (char *)state;
	save_meta();
	state = (uint32_t *)(void *)buf;
	tbl = state + 1;
	type = (int)(state[0] >> 24);
	if (type > 4) type = 0;
	front_i = (int)(state[0] >> 12 & 0xfff);
	rear_i = (int)(state[0] & 0xfff);
	__unlock(&lock);
	return old;
}
