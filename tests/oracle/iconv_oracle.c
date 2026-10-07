/*
 * lib-spfxd oracle test — iconv between Unicode encodings and single-byte
 * character sets, including invalid input, incomplete input, unmappable
 * characters and small output buffers.
  *
 * ORACLE_PORTABLE: the output does not depend on the architecture.
 */
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>

static void conv(const char *to, const char *from, const char *in, size_t inlen, size_t outsz)
{
	iconv_t cd = iconv_open(to, from);
	printf("%s <- %s [%zu,%zu]: ", to, from, inlen, outsz);
	if (cd == (iconv_t)-1) { printf("open failed %d\n", errno); return; }
	char obuf[256], *op = obuf, *ip = (char *)in;
	size_t il = inlen, ol = outsz < sizeof obuf ? outsz : sizeof obuf;
	errno = 0;
	size_t r = iconv(cd, &ip, &il, &op, &ol);
	int e = errno;
	printf("r=%ld err=%d left=%zu out=", (long)r, r == (size_t)-1 ? e : 0, il);
	for (char *p = obuf; p < op; p++) printf("%02x", (unsigned char)*p);
	printf("\n");
	iconv_close(cd);
}

int main(void)
{
	const char *u8 = "h\xc3\xa9llo \xe2\x82\xac \xf0\x9f\x98\x80 \xe6\x97\xa5";
	size_t l = strlen(u8);
	conv("UTF-16LE", "UTF-8", u8, l, 256);
	conv("UTF-16BE", "UTF-8", u8, l, 256);
	conv("UTF-32LE", "UTF-8", u8, l, 256);
	conv("UTF-32BE", "UTF-8", u8, l, 256);
	conv("UCS-4LE", "UTF-8", u8, l, 256);
	conv("UTF-16", "UTF-8", "ab", 2, 256);
	conv("UTF-32", "UTF-8", "ab", 2, 256);
	conv("ISO-8859-1", "UTF-8", "caf\xc3\xa9", 5, 256);
	conv("ISO-8859-1", "UTF-8", u8, l, 256);
	conv("ISO-8859-15", "UTF-8", "\xe2\x82\xac", 3, 256);
	conv("WINDOWS-1252", "UTF-8", "\xe2\x80\x9cq\xe2\x80\x9d", 7, 256);
	conv("KOI8-R", "UTF-8", "\xd0\x9f\xd1\x80\xd0\xb8", 6, 256);
	conv("UTF-8", "KOI8-R", "\xf0\xd2\xc9", 3, 256);
	conv("UTF-8", "ISO-8859-2", "\xa1\xb1\xff", 3, 256);
	conv("UTF-8", "CP437", "\x80\x81\xdb", 3, 256);
	conv("UTF-8", "UTF-16LE", "a\0\x3d\xd8\x00\xde", 6, 256);
	conv("UTF-8", "UTF-16", "\xff\xfe" "a\0b\0", 6, 256);
	conv("UTF-8", "UTF-16", "\xfe\xff" "\0a\0b", 6, 256);
	conv("UTF-8", "UTF-16", "\0a\0b", 4, 256);
	conv("UTF-8", "UTF-32", "\xff\xfe\0\0" "a\0\0\0", 8, 256);
	conv("ASCII", "UTF-8", "abc\xc3\xa9", 5, 256);
	conv("ASCII//TRANSLIT", "UTF-8", "a\xc3\xa9z", 4, 256);
	conv("UTF-8", "UTF-8", "ab\xc3", 3, 256);
	conv("UTF-8", "UTF-8", "ab\xff" "cd", 5, 256);
	conv("UTF-8", "UTF-8", "a\xed\xa0\x80" "b", 5, 256);
	conv("UTF-8", "UTF-8", "a\xc0\xaf" "b", 4, 256);
	conv("UTF-16LE", "UTF-8", u8, l, 5);
	conv("UTF-8", "UTF-16LE", "\x00\xdc", 2, 256);
	conv("UTF-8", "UTF-16LE", "a", 1, 256);
	conv("UTF-8", "ASCII", "a\x80", 2, 256);
	conv("utf8", "latin1", "\xe9", 1, 256);
	conv("UTF-8", "NO-SUCH-CHARSET", "a", 1, 256);
	conv("MACINTOSH", "UTF-8", "\xc3\x84", 2, 256);
	conv("UTF-8", "WINDOWS-1251", "\xcf\xf0\xe8", 3, 256);
	return 0;
}
