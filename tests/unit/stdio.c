/* lib-spfxd test — <stdio.h>: files, buffering, positioning, memory
 * streams, cookies, line input, wide streams, popen. */
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>
#include "t.h"

struct cookie { char buf[64]; size_t len, pos; int closed; };
static ssize_t ck_read(void *c, char *b, size_t n)
{
	struct cookie *k = c;
	size_t m = k->len - k->pos < n ? k->len - k->pos : n;
	memcpy(b, k->buf + k->pos, m);
	k->pos += m;
	return (ssize_t)m;
}
static ssize_t ck_write(void *c, const char *b, size_t n)
{
	struct cookie *k = c;
	if (k->len + n > sizeof k->buf) n = sizeof k->buf - k->len;
	memcpy(k->buf + k->len, b, n);
	k->len += n;
	return (ssize_t)n;
}
static int ck_close(void *c) { ((struct cookie *)c)->closed = 1; return 0; }

int main(void)
{
	char path[] = "/tmp/spfxd_stdioXXXXXX", buf[256];
	int fd = mkstemp(path);
	CHECK(fd >= 0, "mkstemp");
	close(fd);

	/* write, read back, positioning */
	FILE *f = fopen(path, "w+");
	CHECK(f != NULL, "fopen w+");
	CHECK(fputs("line one\nline two\n", f) >= 0 && fprintf(f, "%d %s\n", 3, "three") == 8, "write");
	CHECK(ftell(f) == 26, "ftell after write %ld", ftell(f));
	rewind(f);
	CHECK(fgets(buf, sizeof buf, f) && !strcmp(buf, "line one\n"), "fgets");
	CHECK(fgetc(f) == 'l' && ungetc('X', f) == 'X' && fgetc(f) == 'X', "ungetc");
	CHECK(fseek(f, -6, SEEK_END) == 0 && fgets(buf, sizeof buf, f) && !strcmp(buf, "three\n"), "fseek SEEK_END");
	CHECK(fgetc(f) == EOF && feof(f) && !ferror(f), "EOF flag");
	clearerr(f);
	CHECK(!feof(f), "clearerr");
	fpos_t pos;
	rewind(f);
	fgetpos(f, &pos);
	fgetc(f);
	fsetpos(f, &pos);
	CHECK(fgetc(f) == 'l', "fsetpos");
	/* switching between read and write needs a positioning call */
	fseek(f, 0, SEEK_CUR);
	fputs("Z", f);
	fseek(f, 0, SEEK_SET);
	CHECK(fgetc(f) == 'l' && fgetc(f) == 'Z', "read after write after fseek");
	CHECK(fclose(f) == 0, "fclose");

	/* fread/fwrite of binary data, append mode */
	f = fopen(path, "wb");
	unsigned char bin[1000];
	for (int i = 0; i < 1000; i++) bin[i] = (unsigned char)(i * 7);
	CHECK(fwrite(bin, 1, 1000, f) == 1000, "fwrite");
	fclose(f);
	f = fopen(path, "ab");
	fwrite("tail", 1, 4, f);
	fclose(f);
	f = fopen(path, "rb");
	unsigned char rb[1100];
	CHECK(fread(rb, 1, sizeof rb, f) == 1004 && !memcmp(rb, bin, 1000) && !memcmp(rb + 1000, "tail", 4), "fread + append");
	CHECK(fread(rb, 1, 1, f) == 0 && feof(f), "fread at EOF");
	fclose(f);
	struct stat st;
	CHECK(stat(path, &st) == 0 && st.st_size == 1004, "file size");

	/* modes and errors */
	errno = 0;
	CHECK(!fopen("/nonexistent/dir/x", "r") && errno == ENOENT, "fopen ENOENT");
	errno = 0;
	CHECK(!fopen(path, "q") && errno == EINVAL, "fopen bad mode");
	f = fopen(path, "r");
	CHECK(fputc('x', f) == EOF && ferror(f), "write to read-only stream");
	fclose(f);
	f = fopen(path, "wx");
	CHECK(!f && errno == EEXIST, "fopen x flag");
	f = fopen(path, "re");
	CHECK(f && (fcntl(fileno(f), F_GETFD) & FD_CLOEXEC), "fopen e flag");
	fclose(f);

	/* getline / getdelim */
	f = fopen(path, "w+");
	fputs("short\na much longer line that needs the buffer to grow beyond its initial size .......\nno newline", f);
	rewind(f);
	char *line = 0;
	size_t cap = 0;
	ssize_t n1 = getline(&line, &cap, f), n2 = getline(&line, &cap, f), n3 = getline(&line, &cap, f), n4 = getline(&line, &cap, f);
	CHECK(n1 == 6 && n2 > 60 && n3 == 10 && !strcmp(line, "no newline") && n4 == -1, "getline %zd %zd %zd %zd", n1, n2, n3, n4);
	rewind(f);
	CHECK(getdelim(&line, &cap, ' ', f) == 8 && !strcmp(line, "short\na "), "getdelim");
	free(line);
	fclose(f);

	/* buffering control */
	f = fopen(path, "w");
	char sbuf[32];
	CHECK(setvbuf(f, sbuf, _IOFBF, sizeof sbuf) == 0 && __fbufsize(f) == sizeof sbuf, "setvbuf full");
	fputs("abc", f);
	CHECK(__fpending(f) == 3, "pending output");
	fflush(f);
	CHECK(__fpending(f) == 0, "flushed");
	fclose(f);
	f = fopen(path, "w");
	setvbuf(f, 0, _IONBF, 0);
	fputs("unbuffered", f);
	CHECK(__fpending(f) == 0, "unbuffered writes immediately");
	fclose(f);
	f = fopen(path, "w");
	setvbuf(f, 0, _IOLBF, 64);
	fputs("partial", f);
	CHECK(__fpending(f) == 7 && __flbf(f), "line buffered holds partial line");
	fputs(" done\n", f);
	CHECK(__fpending(f) == 0, "line buffered flushes on newline");
	fclose(f);

	/* fmemopen */
	char mem[16] = "hello world";
	f = fmemopen(mem, sizeof mem, "r+");
	CHECK(f && fgets(buf, 6, f) && !strcmp(buf, "hello"), "fmemopen read");
	fseek(f, 0, SEEK_SET);
	fputs("HE", f);
	fflush(f);
	CHECK(!strncmp(mem, "HEllo", 5), "fmemopen write");
	fclose(f);
	f = fmemopen(0, 10, "w+");
	fputs("tmp", f);
	rewind(f);
	CHECK(fgets(buf, sizeof buf, f) && !strcmp(buf, "tmp"), "fmemopen NULL buffer");
	fclose(f);

	/* open_memstream */
	char *ms = 0;
	size_t ml = 0;
	f = open_memstream(&ms, &ml);
	for (int i = 0; i < 1000; i++) fprintf(f, "%03d", i % 1000);
	fflush(f);
	CHECK(ml == 3000 && !strncmp(ms, "000001002", 9) && !strcmp(ms + 2997, "999"), "open_memstream grows");
	fseek(f, 3, SEEK_SET);
	fputs("X", f);
	fclose(f);
	/* the reported size is the position after the last write (as glibc) */
	CHECK(ms[3] == 'X' && ml == 4, "open_memstream seek/overwrite: ml=%zu", ml);
	free(ms);

	/* fopencookie */
	struct cookie ck = { "cookie data", 11, 0, 0 };
	cookie_io_functions_t io = { ck_read, ck_write, 0, ck_close };
	f = fopencookie(&ck, "r+", io);
	CHECK(f && fgets(buf, sizeof buf, f) && !strcmp(buf, "cookie data"), "fopencookie read");
	fputs("+more", f);
	fclose(f);
	CHECK(ck.closed && ck.len == 16 && !memcmp(ck.buf, "cookie data+more", 16), "fopencookie write/close");

	/* tmpfile, freopen, fdopen */
	f = tmpfile();
	CHECK(f && fputs("tmp", f) >= 0, "tmpfile");
	fclose(f);
	f = fopen(path, "w");
	f = freopen(path, "r", f);
	CHECK(f && fgetc(f) == EOF, "freopen");
	fclose(f);
	fd = open(path, O_RDWR);
	f = fdopen(fd, "r+");
	CHECK(f && fileno(f) == fd, "fdopen");
	fclose(f);
	CHECK(fcntl(fd, F_GETFD) == -1, "fclose closes the descriptor");

	/* sprintf family return values */
	CHECK(snprintf(buf, 5, "%s", "truncated") == 9 && !strcmp(buf, "trun"), "snprintf truncation");
	CHECK(snprintf(0, 0, "%d", 12345) == 5, "snprintf size query");
	char *as;
	CHECK(asprintf(&as, "%s-%d", "x", 7) == 3 && !strcmp(as, "x-7"), "asprintf");
	free(as);

	/* popen */
	f = popen("echo popen works; exit 3", "r");
	CHECK(f && fgets(buf, sizeof buf, f) && !strcmp(buf, "popen works\n"), "popen read");
	int ps = pclose(f);
	CHECK(WIFEXITED(ps) && WEXITSTATUS(ps) == 3, "pclose status %#x", ps);
	f = popen("cat > /dev/null", "w");
	CHECK(f && fputs("x", f) >= 0 && pclose(f) == 0, "popen write");

	/* wide character streams */
	setlocale(LC_CTYPE, "C.UTF-8");
	f = fopen(path, "w+");
	CHECK(fwide(f, 1) > 0, "fwide");
	CHECK(fputws(L"été 中", f) >= 0 && fputwc(L'!', f) == L'!', "fputws");
	rewind(f);
	wchar_t wb[16];
	CHECK(fgetws(wb, 16, f) && !wcscmp(wb, L"été 中!"), "fgetws");
	rewind(f);
	CHECK(fgetwc(f) == 0xe9 && ungetwc(0x1234, f) == 0x1234 && fgetwc(f) == 0x1234, "fgetwc/ungetwc");
	fclose(f);
	f = fopen(path, "r");
	unsigned char raw[16];
	size_t rn = fread(raw, 1, sizeof raw, f);
	CHECK(rn == 10 && raw[0] == 0xc3 && raw[1] == 0xa9, "wide output is UTF-8 (%zu bytes)", rn);
	fclose(f);

	/* remove / rename */
	char path2[64];
	snprintf(path2, sizeof path2, "%s.renamed", path);
	CHECK(rename(path, path2) == 0 && remove(path2) == 0 && access(path2, F_OK) == -1, "rename/remove");
	return DONE();
}
