#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../../include/9deui/9deui.h"

/*
 * Toggle switch (terminal-chic):
 *   - draws a small switch at the right of r
 *   - if label != nil, draws the label on the left
 *
 * This intentionally does NOT draw a “card” background.
 */

void
ui9_toggle_draw(Ui9 *ui, Rectangle r, char *label, int on)
{
	Font *f = ui->font ? ui->font : font;
	int sw = 46, sh = 22;
	int rad = ui->theme.radius;
	Rectangle track, knob;
	Image *tfill;
	Point tp;

	/* Optional label */
	if(label != nil){
		tp = Pt(r.min.x, r.min.y + (Dy(r)-f->height)/2);
		string(ui->dst, tp, ui9img(ui, Ui9CText), ZP, f, label);
	}

	/* Track on the right */
	track = Rect(r.max.x - sw, r.min.y + (Dy(r)-sh)/2, r.max.x, r.min.y + (Dy(r)+sh)/2);

	tfill = on ? ui9img(ui, Ui9CAccent) : ui9img(ui, Ui9CSurface2);
	ui9_roundrect(ui, track, rad, tfill);
	border(ui->dst, track, 1, ui9img(ui, Ui9CBorder), ZP);

	/* Square knob */
	knob = insetrect(track, 3);
	if(on){
		knob.min.x = knob.max.x - (sh-6);
		knob.max.x = track.max.x - 3;
	}else{
		knob.max.x = knob.min.x + (sh-6);
		knob.min.x = track.min.x + 3;
	}
	ui9_roundrect(ui, knob, rad, ui9img(ui, Ui9CSurface));
	border(ui->dst, knob, 1, ui9img(ui, Ui9CBorder), ZP);
}
