#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../../include/9deui/9deui.h"

void
ui9_dropdown_btn_draw(Ui9 *ui, Rectangle r, char *label, int open)
{
	Font *f = ui->font ? ui->font : font;
	USED(open);

	ui9_roundrect(ui, r, ui->theme.radius, ui9img(ui, Ui9CSurface));
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);

	string(ui->dst, Pt(r.min.x+10, r.min.y + (Dy(r)-f->height)/2), ui9img(ui, Ui9CText), ZP, f, label);

	/* caret */
	{
		Point p = Pt(r.max.x-14, r.min.y + Dy(r)/2);
		line(ui->dst, Pt(p.x-5, p.y-2), Pt(p.x, p.y+3), Endsquare, Endsquare, 1, ui9img(ui, Ui9CMuted), ZP);
		line(ui->dst, Pt(p.x, p.y+3), Pt(p.x+5, p.y-2), Endsquare, Endsquare, 1, ui9img(ui, Ui9CMuted), ZP);
	}
}

void
ui9_dropdown_item_draw(Ui9 *ui, Rectangle r, char *label, int selected)
{
	Font *f = ui->font ? ui->font : font;
	Image *fill = selected ? ui9img(ui, Ui9CAccent2) : ui9img(ui, Ui9CSurface);

	draw(ui->dst, r, fill, nil, ZP);
	string(ui->dst, Pt(r.min.x+10, r.min.y + (Dy(r)-f->height)/2), ui9img(ui, Ui9CText), ZP, f, label);
}
