\
#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../include/9deui/9deui.h"

static void
grow(Ui9Flex *fx)
{
	int ncap;
	Ui9FlexItem *p;

	if(fx->n < fx->cap)
		return;
	ncap = fx->cap ? fx->cap*2 : 8;
	p = realloc(fx->it, ncap * sizeof(Ui9FlexItem));
	if(p == nil)
		sysfatal("ui9flex: realloc");
	fx->it = p;
	fx->cap = ncap;
}

void
ui9flex_init(Ui9Flex *fx, Rectangle bounds, int dir, int gap, int pad)
{
	memset(fx, 0, sizeof(*fx));
	fx->bounds = bounds;
	fx->dir = dir;
	fx->gap = gap;
	fx->pad = pad;
}

void
ui9flex_free(Ui9Flex *fx)
{
	if(fx == nil)
		return;
	free(fx->it);
	memset(fx, 0, sizeof(*fx));
}

static int
add(Ui9Flex *fx, int kind, int v, int min)
{
	grow(fx);
	fx->it[fx->n].kind = kind;
	fx->it[fx->n].v = v;
	fx->it[fx->n].min = min;
	fx->it[fx->n].r = Rect(0,0,0,0);
	return fx->n++;
}

int
ui9flex_fixed(Ui9Flex *fx, int px)
{
	return add(fx, Ui9FlexFixed, px, px);
}

int
ui9flex_intrinsic(Ui9Flex *fx, int px)
{
	return add(fx, Ui9FlexIntrinsic, px, px);
}

int
ui9flex_grow(Ui9Flex *fx, int weight, int minpx)
{
	if(weight < 1) weight = 1;
	if(minpx < 0) minpx = 0;
	return add(fx, Ui9FlexGrow, weight, minpx);
}

static int
dim(Rectangle r, int dir)
{
	if(dir == Ui9FlexCol)
		return Dy(r);
	return Dx(r);
}

static void
setrect(Ui9FlexItem *it, Rectangle b, int dir, int off, int len, int pad)
{
	Rectangle r = b;
	if(dir == Ui9FlexCol){
		r.min.y += pad + off;
		r.max.y = r.min.y + len;
		r.min.x += pad;
		r.max.x -= pad;
	}else{
		r.min.x += pad + off;
		r.max.x = r.min.x + len;
		r.min.y += pad;
		r.max.y -= pad;
	}
	it->r = r;
}

void
ui9flex_layout(Ui9Flex *fx)
{
	int i, n, avail, fixed, gap, totalw, off;
	int wsum, w, rem, unit;

	if(fx == nil)
		return;
	n = fx->n;
	if(n <= 0)
		return;

	gap = fx->gap;
	avail = dim(fx->bounds, fx->dir) - 2*fx->pad - gap*(n-1);
	if(avail < 0) avail = 0;

	/* First pass: fixed+intrinsic sizes */
	fixed = 0;
	wsum = 0;
	for(i=0; i<n; i++){
		switch(fx->it[i].kind){
		case Ui9FlexGrow:
			wsum += fx->it[i].v;
			fixed += fx->it[i].min;
			break;
		default:
			fixed += fx->it[i].v;
			break;
		}
	}

	rem = avail - fixed;
	if(rem < 0) rem = 0;

	/* Second pass: assign */
	off = 0;
	for(i=0; i<n; i++){
		switch(fx->it[i].kind){
		case Ui9FlexGrow:
			/* proportional distribution */
			unit = (wsum > 0) ? (rem * fx->it[i].v) / wsum : 0;
			w = fx->it[i].min + unit;
			break;
		default:
			w = fx->it[i].v;
			break;
		}
		setrect(&fx->it[i], fx->bounds, fx->dir, off, w, fx->pad);
		off += w + gap;
	}
}

Rectangle
ui9flex_rect(Ui9Flex *fx, int idx)
{
	if(fx == nil || idx < 0 || idx >= fx->n)
		return Rect(0,0,0,0);
	return fx->it[idx].r;
}
