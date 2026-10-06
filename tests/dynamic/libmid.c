/* test library: depends on libdep; its constructor must run after libdep's */
extern int dep_order[8], dep_norder;
extern int dep_value(void);
int mid_ctor_saw;
__attribute__((constructor)) static void mid_init(void) { mid_ctor_saw = dep_norder; dep_order[dep_norder++] = 2; }
int mid_value(void) { return dep_value() + 1; }
