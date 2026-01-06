#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

int
ui9_runestoutf(char *dst, int ndst, Rune *r, int nr)
{
	char *p = dst;
	char *e = dst + ndst - 1;
	int i;

	for(i=0; i<nr && p < e; i++){
		if(r[i] == 0)
			break;
		p += runetochar(p, &r[i]);
		if(p >= e)
			break;
	}
	*p = 0;
	return p - dst;
}
