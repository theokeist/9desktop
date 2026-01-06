#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../include/9deui/9deui.h"

static ulong
fnv1a(uchar *p, int n)
{
	ulong h = 2166136261UL;
	int i;
	for(i=0; i<n; i++){
		h ^= p[i];
		h *= 16777619UL;
	}
	return h;
}

ulong
ui9_idstr(char *s)
{
	if(s == nil)
		return 0;
	return fnv1a((uchar*)s, strlen(s));
}

ulong
ui9_idptr(void *p)
{
	/* stable-enough within a process; used for ephemeral ids */
	return (ulong)(uintptr)p;
}

void
ui9_input(Ui9 *ui, Mouse *m, Rune k)
{
	if(ui == nil)
		return;
	if(m != nil)
		ui->m = *m;
	ui->k = k;
}

ulong
ui9_focus(Ui9 *ui)
{
	return ui ? ui->focusid : 0;
}

void
ui9_setfocus(Ui9 *ui, ulong id)
{
	if(ui) ui->focusid = id;
}

int
ui9_isfocus(Ui9 *ui, ulong id)
{
	return ui && ui->focusid == id;
}

ulong
ui9_capture(Ui9 *ui)
{
	return ui ? ui->captureid : 0;
}

void
ui9_setcapture(Ui9 *ui, ulong id)
{
	if(ui) ui->captureid = id;
}

int
ui9_iscapture(Ui9 *ui, ulong id)
{
	return ui && ui->captureid == id;
}

void
ui9_releasecapture(Ui9 *ui)
{
	if(ui) ui->captureid = 0;
}

void
ui9_begin(Ui9 *ui, Image *dst)
{
	if(ui == nil)
		return;
	if(dst != nil)
		ui9setdst(ui, dst);
}

void
ui9_end(Ui9 *ui)
{
	USED(ui);
	/* Keep empty on purpose: apps can flushimage(display, 1) when they want. */
}
