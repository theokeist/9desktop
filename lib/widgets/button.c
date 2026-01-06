#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

void
ui9_button_draw(Ui9 *ui, Rectangle r, char *label, int kind, int state)
{
	Font *f = ui->font ? ui->font : font;
	int rad = ui->theme.radius;
	Image *fill, *txt;
	int w;
	Point p;

	/* Fill */
	if(kind == Ui9Primary)
		fill = ui9img(ui, Ui9CAccent);
	else
		fill = ui9img(ui, Ui9CSurface2);

	/* Pressed feedback: flatten to surface2 (no gloss) */
	if(state == Ui9StatePressed)
		fill = ui9img(ui, Ui9CSurface2);

	ui9_roundrect(ui, r, rad, fill);
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);

	/* Text */
	txt = (kind == Ui9Primary) ? ui9img(ui, Ui9CTopbarText) : ui9img(ui, Ui9CText);
	w = stringwidth(f, label);
	p = Pt(r.min.x + (Dx(r)-w)/2, r.min.y + (Dy(r)-f->height)/2);

	string(ui->dst, p, txt, ZP, f, label);
}
