#include <u.h>
#include <libc.h>
#include "../include/9deui/9deui.h"

static void
grow(Ui9Sched *s)
{
	int ncap;
	Ui9Timer *nt;

	ncap = s->cap ? s->cap*2 : 16;
	nt = realloc(s->t, ncap * sizeof(Ui9Timer));
	if(nt == nil)
		sysfatal("ui9sched: realloc failed");
	memset(nt + s->cap, 0, (ncap - s->cap) * sizeof(Ui9Timer));
	s->t = nt;
	s->cap = ncap;
}

vlong
ui9nowms(void)
{
	/* nsec() is monotonic enough for UI animation; convert to ms */
	return nsec() / 1000000LL;
}

void
ui9schedinit(Ui9Sched *s)
{
	memset(s, 0, sizeof *s);
	s->nextid = 1;
}

void
ui9schedfree(Ui9Sched *s)
{
	free(s->t);
	memset(s, 0, sizeof *s);
}

static Ui9Timer*
findid(Ui9Sched *s, int id)
{
	int i;
	for(i=0; i<s->cap; i++)
		if(s->t[i].active && s->t[i].id == id)
			return &s->t[i];
	return nil;
}

int
ui9schedadd(Ui9Sched *s, long delayms, long intervalms, Ui9TimerFn fn, void *arg)
{
	int i;
	Ui9Timer *t;

	if(fn == nil)
		return -1;

	if(s->cap == 0)
		grow(s);

	/* find free slot */
	for(i=0; i<s->cap; i++){
		if(!s->t[i].active){
			t = &s->t[i];
			memset(t, 0, sizeof *t);
			t->active = 1;
			t->id = s->nextid++;
			t->intervalms = intervalms;
			t->repeat = intervalms > 0;
			t->duems = ui9nowms() + delayms;
			t->fn = fn;
			t->arg = arg;
			return t->id;
		}
	}

	/* no free slot; grow and retry */
	grow(s);
	return ui9schedadd(s, delayms, intervalms, fn, arg);
}

void
ui9schedcancel(Ui9Sched *s, int id)
{
	Ui9Timer *t = findid(s, id);
	if(t != nil)
		t->active = 0;
}

long
ui9schednext(Ui9Sched *s, vlong nowms)
{
	int i;
	vlong best, d;

	best = -1;
	for(i=0; i<s->cap; i++){
		if(!s->t[i].active)
			continue;
		d = s->t[i].duems - nowms;
		if(d < 0) d = 0;
		if(best < 0 || d < best)
			best = d;
	}
	if(best < 0)
		return -1;
	if(best > 0x7fffffff)
		return 0x7fffffff;
	return (long)best;
}

int
ui9schedtick(Ui9Sched *s, vlong nowms)
{
	int i, fired;
	Ui9Timer tmp;

	fired = 0;
	for(i=0; i<s->cap; i++){
		if(!s->t[i].active)
			continue;
		if(s->t[i].duems > nowms)
			continue;

		/* copy to avoid re-entrancy issues if callback edits scheduler */
		tmp = s->t[i];

		if(!tmp.repeat){
			s->t[i].active = 0;
		}else{
			/* schedule next */
			s->t[i].duems = nowms + tmp.intervalms;
		}

		tmp.fn(tmp.arg);
		fired++;
	}
	return fired;
}
