#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../../include/9deui/9deui.h"

int
ui9_segment3_hit(Rectangle r[3], Point p)
{
	for(int i=0;i<3;i++)
		if(ptinrect(p, r[i]))
			return i;
	return -1;
}

void
ui9_segment3_draw(Ui9 *ui, Rectangle r[3], char *label[3], int sel)
{
	Font *f = ui->font ? ui->font : font;
	int rad = ui->theme.radius;

	for(int i=0;i<3;i++){
		Image *fill = (i == sel) ? ui9img(ui, Ui9CAccent2) : ui9img(ui, Ui9CSurface2);
		Image *txt  = (i == sel) ? ui9img(ui, Ui9CText)   : ui9img(ui, Ui9CMuted);
		int w;
		Point p;

		ui9_roundrect(ui, r[i], rad, fill);
		border(ui->dst, r[i], 1, ui9img(ui, Ui9CBorder), ZP);

		w = stringwidth(f, label[i]);
		p = Pt(r[i].min.x + (Dx(r[i])-w)/2, r[i].min.y + (Dy(r[i])-f->height)/2);
		string(ui->dst, p, txt, ZP, f, label[i]);
	}
}
