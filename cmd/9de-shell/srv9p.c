#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9p.h>

#include "srv9p.h"

/*
 * /srv/9de (v0)
 *
 * Files:
 *   ctl    (write commands)
 *   status (read state)
 *   events (read line stream)
 *
 * This is intentionally small: lock down the interface early.
 */

static Srv fs;
static Tree *t;

static char preset[32] = "terminal";
static char panel[16]  = "top";

/* event queue (simple ring of small lines) */
enum { QN = 128, QL = 160 };
static char q[QN][QL];
static int qh, qt;

/* pending readers for /events */
static Req *pend;

char*
srv9preset(void){ return preset; }

char*
srv9panel(void){ return panel; }

static void
qpush(char *s)
{
	Req *r;
	char *p;
	int n;

	/* if a reader is waiting, respond immediately */
	if(pend != nil){
		r = pend;
		pend = nil;
		p = s;
		n = strlen(p);
		if(n > r->ifcall.count)
			n = r->ifcall.count;
		r->ofcall.count = n;
		memmove(r->ofcall.data, p, n);
		respond(r, nil);
		return;
	}

	strecpy(q[qt], q[qt]+QL, s);
	qt = (qt+1) % QN;
	if(qt == qh) /* drop oldest */
		qh = (qh+1) % QN;
}

void
srv9ppostevent(char *fmt, ...)
{
	va_list ap;
	char buf[QL];

	va_start(ap, fmt);
	vseprint(buf, buf+sizeof buf, fmt, ap);
	va_end(ap);

	/* ensure newline (events are line-based) */
	if(buf[0] == 0)
		return;
	if(buf[strlen(buf)-1] != '\n')
		strecpy(buf+strlen(buf), buf+sizeof buf, "\n");
	qpush(buf);
}

static void
ctlwrite(Req *r)
{
	char *s, *a;
	char buf[256];

	/* copy and NUL-terminate */
	snprint(buf, sizeof buf, "%.*s", (int)r->ifcall.count, (char*)r->ifcall.data);
	s = buf;

	/* trim leading spaces */
	while(*s==' ' || *s=='\t') s++;
	a = strchr(s, ' ');
	if(a != nil){
		*a++ = 0;
		while(*a==' ' || *a=='\t') a++;
	}

	if(strcmp(s, "ping")==0){
		srv9ppostevent("ok ping");
	}else if(strcmp(s, "reload")==0){
		srv9ppostevent("ok reload");
	}else if(strcmp(s, "apply")==0){
		srv9ppostevent("ok apply");
	}else if(strcmp(s, "setpreset")==0){
		if(a==nil){
			srv9ppostevent("err setpreset missing");
		}else if(strcmp(a,"terminal")==0 || strcmp(a,"dark")==0 || strcmp(a,"glass")==0){
			strecpy(preset, preset+sizeof preset, a);
			srv9ppostevent("ok setpreset %s", preset);
		}else{
			srv9ppostevent("err setpreset badvalue");
		}
	}else if(strcmp(s, "panel")==0){
		if(a==nil){
			srv9ppostevent("err panel missing");
		}else if(strcmp(a,"top")==0 || strcmp(a,"bottom")==0 || strcmp(a,"left")==0){
			strecpy(panel, panel+sizeof panel, a);
			srv9ppostevent("ok panel %s", panel);
		}else{
			srv9ppostevent("err panel badvalue");
		}
	}else{
		srv9ppostevent("err unknown");
	}

	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
}

static void
statusread(Req *r)
{
	char buf[256];
	int n;

	n = snprint(buf, sizeof buf, "preset %s\npanel %s\n", preset, panel);

	readstr(r, buf);
	respond(r, nil);
}

static void
eventsread(Req *r)
{
	char *s;
	int n;

	/* treat as stream: only offset 0 is meaningful */
	if(r->ifcall.offset != 0){
		r->ofcall.count = 0;
		respond(r, nil);
		return;
	}

	if(qh == qt){
		/* nothing available: hold request */
		pend = r;
		return;
	}

	s = q[qh];
	qh = (qh+1) % QN;

	n = strlen(s);
	if(n > r->ifcall.count)
		n = r->ifcall.count;
	r->ofcall.count = n;
	memmove(r->ofcall.data, s, n);
	respond(r, nil);
}

static void
fsread(Req *r)
{
	if(strcmp(r->fid->file->name, "status")==0)
		statusread(r);
	else if(strcmp(r->fid->file->name, "events")==0)
		eventsread(r);
	else
		respond(r, "permission denied");
}

static void
fswrite(Req *r)
{
	if(strcmp(r->fid->file->name, "ctl")==0)
		ctlwrite(r);
	else
		respond(r, "permission denied");
}

void
srv9pstart(void)
{
	fs.read = fsread;
	fs.write = fswrite;

	t = alloctree(nil, nil, DMDIR|0555, nil);
	createfile(t->root, "ctl", nil, 0666, nil);
	createfile(t->root, "status", nil, 0444, nil);
	createfile(t->root, "events", nil, 0444, nil);
	fs.tree = t;

	/* post at /srv/9de; do not auto-mount (Settings/Dash can mount) */
	threadpostmountsrv(&fs, "9de", nil, 0);

	srv9ppostevent("ok boot");
}
