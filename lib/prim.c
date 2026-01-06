#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

void
ui9_roundrect(Ui9 *ui, Rectangle r, int rad, Image *fill)
{
	Rectangle old;
	Point c;

	if(rad <= 0){
		draw(ui->dst, r, fill, nil, ZP);
		return;
	}

	/* for C89/6c: declare loop counters up front */

	old = ui->dst->clipr;
	replclipr(ui->dst, 0, r);

	/* middle + sides */
	draw(ui->dst, Rect(r.min.x+rad, r.min.y, r.max.x-rad, r.max.y), fill, nil, ZP);
	draw(ui->dst, Rect(r.min.x, r.min.y+rad, r.min.x+rad, r.max.y-rad), fill, nil, ZP);
	draw(ui->dst, Rect(r.max.x-rad, r.min.y+rad, r.max.x, r.max.y-rad), fill, nil, ZP);

	/* corners */
	c = Pt(r.min.x+rad, r.min.y+rad);
	fillellipse(ui->dst, c, rad, rad, fill, ZP);
	c = Pt(r.max.x-rad-1, r.min.y+rad);
	fillellipse(ui->dst, c, rad, rad, fill, ZP);
	c = Pt(r.min.x+rad, r.max.y-rad-1);
	fillellipse(ui->dst, c, rad, rad, fill, ZP);
	c = Pt(r.max.x-rad-1, r.max.y-rad-1);
	fillellipse(ui->dst, c, rad, rad, fill, ZP);

	replclipr(ui->dst, 0, old);
}

void
ui9_shadowstring(Ui9 *ui, Point p, char *s)
{
	Font *f = ui->font ? ui->font : font;
	Image *shadow = ui9img(ui, Ui9CShadow);
	Image *text = ui9img(ui, Ui9CText);

	string(ui->dst, addpt(p, Pt(0,1)), shadow, ZP, f, s);
	string(ui->dst, p, text, ZP, f, s);
}

void
ui9_shadowstring_center(Ui9 *ui, Rectangle r, char *s)
{
	Font *f = ui->font ? ui->font : font;
	int w = stringwidth(f, s);
	Point p = Pt(r.min.x + (Dx(r)-w)/2, r.min.y + (Dy(r)-f->height)/2);
	ui9_shadowstring(ui, p, s);
}

void
ui9_card(Ui9 *ui, Rectangle r, int rad)
{
	ui9_roundrect(ui, r, rad, ui9img(ui, Ui9CSurface));
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);
}

void
ui9_card2(Ui9 *ui, Rectangle r, int rad)
{
	ui9_roundrect(ui, r, rad, ui9img(ui, Ui9CSurface2));
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);
}
