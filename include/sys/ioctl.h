/* lib-spfxd — <sys/ioctl.h> (Linux generic ioctl numbers used by x86-64) */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H
#include <features.h>

#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U
#define _IOC(a, b, c, d) (((a) << 30) | ((b) << 8) | (c) | ((d) << 16))
#define _IO(a, b)      _IOC(_IOC_NONE, (a), (b), 0)
#define _IOW(a, b, c)  _IOC(_IOC_WRITE, (a), (b), sizeof(c))
#define _IOR(a, b, c)  _IOC(_IOC_READ, (a), (b), sizeof(c))
#define _IOWR(a, b, c) _IOC(_IOC_READ | _IOC_WRITE, (a), (b), sizeof(c))

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TCGETA     0x5405
#define TCSETA     0x5406
#define TCSETAW    0x5407
#define TCSETAF    0x5408
#define TCSBRK     0x5409
#define TCXONC     0x540A
#define TCFLSH     0x540B
#define TIOCEXCL   0x540C
#define TIOCNXCL   0x540D
#define TIOCSCTTY  0x540E
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCOUTQ   0x5411
#define TIOCSTI    0x5412
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCMGET   0x5415
#define TIOCMBIS   0x5416
#define TIOCMBIC   0x5417
#define TIOCMSET   0x5418
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD   0x541B
#define TIOCINQ    FIONREAD
#define TIOCLINUX  0x541C
#define TIOCCONS   0x541D
#define TIOCGSERIAL 0x541E
#define TIOCSSERIAL 0x541F
#define TIOCPKT    0x5420
#define FIONBIO    0x5421
#define TIOCNOTTY  0x5422
#define TIOCSETD   0x5423
#define TIOCGETD   0x5424
#define TCSBRKP    0x5425
#define TIOCSBRK   0x5427
#define TIOCCBRK   0x5428
#define TIOCGSID   0x5429
#define TIOCGPTN   0x80045430
#define TIOCSPTLCK 0x40045431
#define TIOCGPTPEER 0x5441
#define FIONCLEX   0x5450
#define FIOCLEX    0x5451
#define FIOASYNC   0x5452

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

__SPFXD_BEGIN_DECLS
int ioctl(int, unsigned long, ...);
__SPFXD_END_DECLS
#endif
