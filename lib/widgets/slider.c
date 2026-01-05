#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../../include/9deui/9deui.h"

static int
clampi(int v, int lo, int hi)
{
	if(v < lo) return lo;
	if(v > hi) return hi;
	return v;
}

void
ui9_slider_draw(Ui9 *ui, Rectangle r, int value)
{
	int v = clampi(value, 0, 255);
	int y = r.min.y + Dy(r)/2;
	int x1 = r.min.x;
	int x2 = r.max.x;
	int w = x2 - x1;
	int fx = x1 + (w * v)/255;

	/* base track */
	line(ui->dst, Pt(x1, y), Pt(x2, y), Endsquare, Endsquare, 3, ui9img(ui, Ui9CBorder), ZP);

	/* filled track */
	line(ui->dst, Pt(x1, y), Pt(fx, y), Endsquare, Endsquare, 3, ui9img(ui, Ui9CAccent), ZP);

	/* ticks (subtle) */
	int t;
	for(t=0; t<=4; t++){
		int tx = x1 + (w*t)/4;
		line(ui->dst, Pt(tx, y-7), Pt(tx, y-4), Endsquare, Endsquare, 1, ui9img(ui, Ui9CBorder), ZP);
	}

	/* knob */
	{
		int kw = 10, kh = 18;
		Rectangle k = Rect(fx-kw/2, y-kh/2, fx+kw/2, y+kh/2);
		ui9_roundrect(ui, k, ui->theme.radius, ui9img(ui, Ui9CSurface));
		border(ui->dst, k, 1, ui9img(ui, Ui9CBorder), ZP);
	}
}

int
ui9_slider_value(Rectangle r, Point p)
{
	int x1 = r.min.x;
	int x2 = r.max.x;
	int w = x2 - x1;
	int v;

	if(w <= 0)
		return 0;

	v = ((p.x - x1) * 255) / w;
	return clampi(v, 0, 255);
}
