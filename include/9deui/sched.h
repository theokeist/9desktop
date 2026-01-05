#ifndef _9DEUI_SCHED_H_
#define _9DEUI_SCHED_H_

/*
 * ui9 scheduler (lightweight, no threads required)
 *
 * - You call ui9schedtick() regularly from the UI loop.
 * - Timers can be one-shot or repeating.
 * - Designed for spinners, toasts, coalesced redraw, and “apply later” commits.
 *
 * Time base:
 *   nowms is milliseconds; use ui9nowms().
 */

typedef struct Ui9Sched Ui9Sched;
typedef struct Ui9Timer Ui9Timer;

typedef void (*Ui9TimerFn)(void *arg);

struct Ui9Timer {
	int id;
	int active;
	int repeat;        /* 1 if intervalms > 0 */
	long intervalms;   /* >0 for repeating */
	vlong duems;       /* next fire time (ms) */
	Ui9TimerFn fn;
	void *arg;
};

struct Ui9Sched {
	int nextid;
	int nt;
	int cap;
	Ui9Timer *t;
};

/* monotonic-ish “now” in ms (based on nsec()) */
vlong ui9nowms(void);

/* init/free */
void ui9schedinit(Ui9Sched *s);
void ui9schedfree(Ui9Sched *s);

/* add timer: delayms from now; intervalms=0 for one-shot; returns timer id */
int  ui9schedadd(Ui9Sched *s, long delayms, long intervalms, Ui9TimerFn fn, void *arg);

/* cancel a timer id (safe to call multiple times) */
void ui9schedcancel(Ui9Sched *s, int id);

/* run due timers; returns how many fired */
int  ui9schedtick(Ui9Sched *s, vlong nowms);

/* ms until next timer due; returns -1 if none */
long ui9schednext(Ui9Sched *s, vlong nowms);

#endif
