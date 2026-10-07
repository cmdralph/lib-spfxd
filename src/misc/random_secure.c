/*
 * lib-spfxd — cryptographically secure randomness.
 *
 * getrandom/getentropy go straight to the kernel.  arc4random is a ChaCha20
 * keystream generator (RFC 8439 block function) keyed from getrandom: each
 * refill produces 1 KiB of output, then immediately re-keys from the first
 * 40 bytes of fresh keystream (key + nonce), so earlier output cannot be
 * recomputed from a later state ("fast key erasure").  The state is reset
 * whenever the process id changes, so a forked child never repeats its
 * parent's stream, and the generator is re-seeded from the kernel every
 * 1.6 MB of output.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>
#include "lock.h"

ssize_t getrandom(void *buf, size_t n, unsigned flags)
{
	return __sysret_cp(SYS_getrandom, buf, n, flags);
}

int getentropy(void *buf, size_t n)
{
	unsigned char *p = buf;
	if (n > 256) {
		errno = EIO;
		return -1;
	}
	while (n) {
		long r = __syscall(SYS_getrandom, p, n, 0);
		if (r == -EINTR) continue;
		if (r < 0) {
			errno = (int)-r;
			return -1;
		}
		p += r;
		n -= (size_t)r;
	}
	return 0;
}

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define QR(a, b, c, d) ( \
	a += b, d ^= a, d = ROTL(d, 16), \
	c += d, b ^= c, b = ROTL(b, 12), \
	a += b, d ^= a, d = ROTL(d, 8), \
	c += d, b ^= c, b = ROTL(b, 7))

static void chacha20_block(uint32_t out[16], const uint32_t in[16])
{
	uint32_t x[16];
	memcpy(x, in, sizeof x);
	for (int i = 0; i < 10; i++) {
		QR(x[0], x[4], x[8], x[12]);
		QR(x[1], x[5], x[9], x[13]);
		QR(x[2], x[6], x[10], x[14]);
		QR(x[3], x[7], x[11], x[15]);
		QR(x[0], x[5], x[10], x[15]);
		QR(x[1], x[6], x[11], x[12]);
		QR(x[2], x[7], x[8], x[13]);
		QR(x[3], x[4], x[9], x[14]);
	}
	for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

#define BUFBYTES 1024
#define RESEED_BYTES (1600 * 1024)

static struct {
	uint32_t key[8], nonce[3];
	unsigned char buf[BUFBYTES];
	size_t avail;
	size_t since_seed;
	pid_t pid;
} rs;
static volatile int rs_lock;

static void rekey(const unsigned char seed[44])
{
	memcpy(rs.key, seed, 32);
	memcpy(rs.nonce, seed + 32, 12);
}

static void seed_from_kernel(void)
{
	unsigned char seed[44];
	if (getentropy(seed, sizeof seed)) {
		/* The kernel interface is mandatory on every supported kernel;
		 * refusing to continue is safer than producing weak output. */
		abort();
	}
	rekey(seed);
	explicit_bzero(seed, sizeof seed);
	rs.avail = 0;
	rs.since_seed = 0;
	rs.pid = getpid();
}

static void refill(void)
{
	uint32_t in[16] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };
	memcpy(in + 4, rs.key, 32);
	memcpy(in + 13, rs.nonce, 12);
	uint32_t words[(BUFBYTES + 64) / 4];
	for (uint32_t blk = 0; blk < (BUFBYTES + 64) / 64; blk++) {
		in[12] = blk;
		chacha20_block(words + 16 * blk, in);
	}
	unsigned char *tmp = (unsigned char *)words;
	/* first 44 bytes become the next key; the rest is output */
	rekey(tmp);
	memcpy(rs.buf, tmp + 64, BUFBYTES);
	explicit_bzero(words, sizeof words);
	explicit_bzero(in, sizeof in);
	rs.avail = BUFBYTES;
}

static void take(unsigned char *out, size_t n)
{
	if (rs.pid != getpid() || rs.since_seed > RESEED_BYTES) seed_from_kernel();
	while (n) {
		if (!rs.avail) refill();
		size_t k = n < rs.avail ? n : rs.avail;
		unsigned char *src = rs.buf + BUFBYTES - rs.avail;
		memcpy(out, src, k);
		memset(src, 0, k);   /* used output never stays in memory */
		rs.avail -= k;
		rs.since_seed += k;
		out += k;
		n -= k;
	}
}

void arc4random_buf(void *buf, size_t n)
{
	__lock(&rs_lock);
	take(buf, n);
	__unlock(&rs_lock);
}

unsigned arc4random(void)
{
	unsigned v;
	arc4random_buf(&v, sizeof v);
	return v;
}

/* Uniform in [0, bound) without modulo bias (rejection sampling). */
unsigned arc4random_uniform(unsigned bound)
{
	if (bound < 2) return 0;
	unsigned min = -bound % bound;   /* 2^32 mod bound */
	unsigned r;
	do r = arc4random(); while (r < min);
	return r % bound;
}
