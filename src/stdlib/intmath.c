/* lib-spfxd — abs / labs / llabs / imaxabs / div family. */
#include <stdlib.h>
#include <inttypes.h>

int abs(int a) { return a < 0 ? -a : a; }
long labs(long a) { return a < 0 ? -a : a; }
long long llabs(long long a) { return a < 0 ? -a : a; }
intmax_t imaxabs(intmax_t a) { return a < 0 ? -a : a; }

div_t div(int n, int d) { return (div_t){ n / d, n % d }; }
ldiv_t ldiv(long n, long d) { return (ldiv_t){ n / d, n % d }; }
lldiv_t lldiv(long long n, long long d) { return (lldiv_t){ n / d, n % d }; }
imaxdiv_t imaxdiv(intmax_t n, intmax_t d) { return (imaxdiv_t){ n / d, n % d }; }
