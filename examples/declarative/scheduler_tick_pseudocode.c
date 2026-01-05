/*
 * Example: integrate Ui9Sched into a libevent-driven UI loop (pseudo-code).
 *
 * Key idea:
 *   - schedule timers (spinner tick, coalesced redraw)
 *   - set alarm(nextms) to wake the process out of event()
 *   - on wake: run scheduler tick, then redraw if needed
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <9deui/9deui.h>

static Ui9Sched sched;

static int dirty;

static void
redraw(void *arg)
{
	USED(arg);
	dirty = 1;
}

static void
spintick(void *arg)
{
	USED(arg);
	/* update spinner phase in your model here */
	dirty = 1;
}

void
main(void)
{
	Event e;
	vlong now;
	long next;

	einit(Emouse|Ekeyboard);

	ui9schedinit(&sched);

	/* coalesce redraw: schedule redraw at most once per ~16ms */
	ui9schedadd(&sched, 0, 16, redraw, nil);

	/* spinner at ~25fps */
	ui9schedadd(&sched, 0, 40, spintick, nil);

	for(;;){
		now = ui9nowms();
		next = ui9schednext(&sched, now);
		if(next < 0) next = 200; /* idle wake */
		alarm(next);

		/* blocks until input or alarm */
		if(event(&e) >= 0){
			/* handle input; mark dirty if state changes */
		}

		/* run due timers */
		ui9schedtick(&sched, ui9nowms());

		if(dirty){
			/* draw frame */
			dirty = 0;
		}
	}
}
