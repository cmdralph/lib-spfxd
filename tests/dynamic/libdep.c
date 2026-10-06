/* test library: a dependency with TLS, a constructor and exported data */
#include <stdio.h>
int dep_counter = 100;                 /* copy-relocated into the program */
__thread int dep_tls = 7;              /* general-dynamic TLS */
static __thread int dep_tls_array[64] = { [5] = 55 };
int dep_order[8], dep_norder;
__attribute__((constructor)) static void dep_init(void) { dep_order[dep_norder++] = 1; }
int dep_get_tls(void) { return dep_tls + dep_tls_array[5]; }
int *dep_tls_addr(void) { return &dep_tls; }
int dep_value(void) { return dep_counter; }
