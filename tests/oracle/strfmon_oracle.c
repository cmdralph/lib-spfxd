#include <monetary.h>
#include <stdio.h>
#include <locale.h>
int main(void){ setlocale(LC_ALL,"C"); char b[100];
 const char *f[]={"[%n]","[%i]","[%11n]","[%-11n]","[%#5n]","[%=*#5n]","[%(n]","[%(#5n]","[%.0n]","[%.3n]","[%^n]","[%!n]","[%+n]","[%%]"};
 double v[]={123.45,-123.45,0.005,1234567.891};
 for(unsigned i=0;i<sizeof f/sizeof*f;i++) for(int j=0;j<4;j++){ ssize_t r=strfmon(b,sizeof b,f[i],v[j]); printf("%s %g -> %zd %s\n",f[i],v[j],r,r>=0?b:"ERR"); }
 return 0; }
