/*
 * lib-spfxd — CPU feature detection (portable fallback: no optional
 * features; architecture code that dispatches on __cpu_features always
 * has a baseline path).
 */
#include "libc.h"

hidden unsigned __cpu_features;

hidden void __init_cpu(void)
{
}
