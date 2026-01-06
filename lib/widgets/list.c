#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

void
ui9_listitem_draw(Ui9 *ui, Rectangle r, char *label, int selected)
{
	Font *f = ui->font ? ui->font : font;
	Image *fill = selected ? ui9img(ui, Ui9CAccent2) : ui9img(ui, Ui9CSurface);

	ui9_roundrect(ui, r, ui->theme.radius, fill);
	border(ui->dst, r, 1, ui9img(ui, Ui9CBorder), ZP);

	string(ui->dst, Pt(r.min.x+10, r.min.y + (Dy(r)-f->height)/2), ui9img(ui, Ui9CText), ZP, f, label);
}
