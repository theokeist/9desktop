#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

static int
clampi(int v, int lo, int hi)
{
	if(v < lo) return lo;
	if(v > hi) return hi;
	return v;
}

void
ui9_progress_draw(Ui9 *ui, Rectangle r, int pct)
{
	Rectangle fill;
	int rad = ui->theme.radius;

	pct = clampi(pct, 0, 100);

	ui9_roundrect(ui, r, rad, ui9img(ui, Ui9CSurface2));
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);

	fill = insetrect(r, 2);
	fill.max.x = fill.min.x + Dx(fill) * pct / 100;
	if(Dx(fill) > 0)
		ui9_roundrect(ui, fill, rad, ui9img(ui, Ui9CAccent));
}
