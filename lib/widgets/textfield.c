#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../../include/9deui/9deui.h"

void
ui9_textfield_draw(Ui9 *ui, Rectangle r, Rune *buf, int nbuf, int focused, char *placeholder)
{
	Font *f = ui->font ? ui->font : font;
	char tmp[512];
	Rectangle inner;
	int w;
	Image *b;

	ui9_roundrect(ui, r, ui->theme.radius, ui9img(ui, Ui9CSurface));
	b = focused ? ui9img(ui, Ui9CAccent) : ui9img(ui, Ui9CBorder);
	border(ui->dst, r, focused ? 2 : 1, b, ZP);

	inner = insetrect(r, 8);

	ui9_runestoutf(tmp, sizeof tmp, buf, nbuf);

	if(tmp[0] == 0 && placeholder != nil)
		string(ui->dst, addpt(inner.min, Pt(0, (Dy(inner)-f->height)/2)), ui9img(ui, Ui9CMuted), ZP, f, placeholder);
	else
		string(ui->dst, addpt(inner.min, Pt(0, (Dy(inner)-f->height)/2)), ui9img(ui, Ui9CText), ZP, f, tmp);

	if(focused){
		w = stringwidth(f, tmp);
		{
			Point c0 = addpt(inner.min, Pt(w+2, (Dy(inner)-f->height)/2));
			line(ui->dst, addpt(c0, Pt(0,2)), addpt(c0, Pt(0,f->height-2)),
			     Endsquare, Endsquare, 1, ui9img(ui, Ui9CText), ZP);
		}
	}
}
