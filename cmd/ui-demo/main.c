#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <9deui/9deui.h>

/*
 * ui9demo — 9DE UI components demo (terminal-chic baseline)
 *
 * This is not “a Control Center” yet.
 * It is a living demo window that:
 *   - exercises core widgets
 *   - shows alpha/blending preview
 *   - provides a predictable layout contract we can reuse later
 *
 * Keys:
 *   q   quit
 */

static Ui9 ui;

/* state */
static int navsel;
static int chkpanel = 1;
static int chkdemo  = 1;
static int alpha_v  = 180;
static int segsel   = 0;

static Rune textbuf[128];
static int  textn;
static int  focused;

/* layout rects */
static Rectangle rnav, rmain, rhead, rcontent;
static Rectangle navitem[4];

static Rectangle row1, row2, row3, row4, row5;
static Rectangle chk1, chk2;
static Rectangle rslider;
static Rectangle rtext;
static Rectangle rseg[3];
static Rectangle rpreview;
static Rectangle rout;

static int
clampi(int v, int lo, int hi)
{
	if(v < lo) return lo;
	if(v > hi) return hi;
	return v;
}

static void
layout(void)
{
	Rectangle sr = screen->r;
	int navw = 220;
	int headh = 38;
	int pad = 12;

	rnav  = Rect(sr.min.x, sr.min.y, sr.min.x + navw, sr.max.y);
	rmain = Rect(rnav.max.x, sr.min.y, sr.max.x, sr.max.y);

	/* left nav items */
	navitem[0] = Rect(rnav.min.x+12, rnav.min.y+14, rnav.max.x-12, rnav.min.y+14+34);
	navitem[1] = Rect(rnav.min.x+12, navitem[0].max.y+6, rnav.max.x-12, navitem[0].max.y+6+34);
	navitem[2] = Rect(rnav.min.x+12, navitem[1].max.y+6, rnav.max.x-12, navitem[1].max.y+6+34);
	navitem[3] = Rect(rnav.min.x+12, navitem[2].max.y+6, rnav.max.x-12, navitem[2].max.y+6+34);

	rhead = Rect(rmain.min.x, rmain.min.y, rmain.max.x, rmain.min.y + headh);
	rcontent = Rect(rmain.min.x, rhead.max.y, rmain.max.x, rmain.max.y);

	/* content card */
	rcontent = insetrect(rcontent, pad);
	rcontent.min.y += 6;

	/* rows */
	{
		int y = rcontent.min.y + 12;
		int rowh = 46;

		row1 = Rect(rcontent.min.x+12, y, rcontent.max.x-12, y+rowh); y += rowh;
		row2 = Rect(rcontent.min.x+12, y, rcontent.max.x-12, y+rowh); y += rowh;
		row3 = Rect(rcontent.min.x+12, y, rcontent.max.x-12, y+rowh); y += rowh;
		row4 = Rect(rcontent.min.x+12, y, rcontent.max.x-12, y+rowh); y += rowh;
		row5 = Rect(rcontent.min.x+12, y, rcontent.max.x-12, y+rowh); y += rowh;

		/* controls at the right */
		chk1 = Rect(row1.max.x-22, row1.min.y + (Dy(row1)-16)/2, row1.max.x-6, row1.min.y + (Dy(row1)+16)/2);
		chk2 = Rect(row2.max.x-22, row2.min.y + (Dy(row2)-16)/2, row2.max.x-6, row2.min.y + (Dy(row2)+16)/2);

		rslider = Rect(row3.max.x-220, row3.min.y + (Dy(row3)-22)/2, row3.max.x-12, row3.min.y + (Dy(row3)+22)/2);

		rtext = Rect(row4.max.x-260, row4.min.y + (Dy(row4)-30)/2, row4.max.x-12, row4.min.y + (Dy(row4)+30)/2);

		/* 3-segment control */
		{
			int w = 72;
			int h = 28;
			int x = row5.max.x - 12 - (w*3 + 8);
			int y0 = row5.min.y + (Dy(row5)-h)/2;
				int i;
				for(i=0; i<3; i++)
					rseg[i] = Rect(x + i*(w+4), y0, x + i*(w+4) + w, y0+h);
			}

		/* alpha preview box under slider */
		rpreview = Rect(rslider.min.x, row3.max.y + 10, rslider.max.x, row3.max.y + 10 + 54);

		/* output area */
		rout = Rect(rcontent.min.x+12, rpreview.max.y + 14, rcontent.max.x-12, rcontent.max.y-12);
	}
}

static void
draw_checkbox(Rectangle r, int on)
{
	ui9_roundrect(&ui, r, ui.theme.radius, ui9img(&ui, Ui9CSurface));
	border(screen, r, 1, ui9img(&ui, Ui9CBorder), ZP);

	if(on){
		/* tick */
		Point a = Pt(r.min.x+4, r.min.y + Dy(r)/2);
		Point b = Pt(r.min.x+7, r.max.y-4);
		Point c = Pt(r.max.x-3, r.min.y+4);
		line(screen, a, b, Endsquare, Endsquare, 2, ui9img(&ui, Ui9CAccent), ZP);
		line(screen, b, c, Endsquare, Endsquare, 2, ui9img(&ui, Ui9CAccent), ZP);
	}
}

static void
draw_row(Rectangle r, char *title, char *desc)
{
	Font *f = ui.font ? ui.font : font;

	/* separator */
	line(screen, Pt(r.min.x, r.max.y-1), Pt(r.max.x, r.max.y-1),
	     Endsquare, Endsquare, 1, ui9img(&ui, Ui9CBorder), ZP);

	/* title */
	string(screen, Pt(r.min.x, r.min.y + 10), ui9img(&ui, Ui9CText), ZP, f, title);

	/* desc */
	if(desc != nil)
		string(screen, Pt(r.min.x+170, r.min.y + 10), ui9img(&ui, Ui9CMuted), ZP, f, desc);
}

static void
draw_checker(Rectangle r)
{
	int s = 8;
	int y, x;

	for(y=r.min.y; y<r.max.y; y+=s){
		for(x=r.min.x; x<r.max.x; x+=s){
			Rectangle b = Rect(x, y, x+s, y+s);
			Image *fill = (((x/s) + (y/s)) & 1) ? ui9img(&ui, Ui9CSurface2) : ui9img(&ui, Ui9CSurface);
			draw(screen, b, fill, nil, ZP);
		}
	}
	border(screen, r, 1, ui9img(&ui, Ui9CBorder), ZP);
}

static void
draw_alpha_overlay(Rectangle r, int a)
{
	Image *ov;
	ov = allocimage(display, Rect(0,0,1,1), RGBA32, 1, setalpha(ui.theme.accentrgb, (uchar)clampi(a,0,255)));
	if(ov != nil){
		draw(screen, r, ov, nil, ZP);
		freeimage(ov);
	}
}

static void
redraw(void)
{
	Font *f = ui.font ? ui.font : font;
	char buf[128];

	layout();

	/* window background */
	draw(screen, screen->r, ui9img(&ui, Ui9CBg), nil, ZP);

	/* left nav panel */
	{
		char *items[4] = { "Controls", "Window", "Components", "About" };
		Image *fill;
		Image *txt;
		int i;

		ui9_roundrect(&ui, insetrect(rnav, 10), ui.theme.radius, ui9img(&ui, Ui9CSurface));
		border(screen, insetrect(rnav, 10), 1, ui9img(&ui, Ui9CBorder), ZP);

		for(i=0; i<4; i++){
			Rectangle it = navitem[i];
			fill = (i==navsel) ? ui9img(&ui, Ui9CTopbarBg) : ui9img(&ui, Ui9CSurface);
			txt  = (i==navsel) ? ui9img(&ui, Ui9CTopbarText) : ui9img(&ui, Ui9CText);

			ui9_roundrect(&ui, it, ui.theme.radius, fill);
			if(i==navsel)
				border(screen, it, 1, ui9img(&ui, Ui9CBorder), ZP);

			string(screen, Pt(it.min.x+10, it.min.y + (Dy(it)-f->height)/2), txt, ZP, f, items[i]);
		}
	}

	/* main header */
	{
		draw(screen, rhead, ui9img(&ui, Ui9CSurface2), nil, ZP);
		line(screen, Pt(rhead.min.x, rhead.max.y-1), Pt(rhead.max.x, rhead.max.y-1),
		     Endsquare, Endsquare, 1, ui9img(&ui, Ui9CBorder), ZP);

		string(screen, Pt(rhead.min.x+14, rhead.min.y + (Dy(rhead)-f->height)/2),
		       ui9img(&ui, Ui9CText), ZP, f, "ui9demo");

		/* right actions (visual only for now) */
		{
			char *a1="Reset", *a2="Apply";
			int w2 = stringwidth(f, a2);
			int w1 = stringwidth(f, a1);
			Point p2 = Pt(rhead.max.x-14-w2, rhead.min.y + (Dy(rhead)-f->height)/2);
			Point p1 = Pt(p2.x-18-w1, p2.y);
			string(screen, p1, ui9img(&ui, Ui9CMuted), ZP, f, a1);
			string(screen, p2, ui9img(&ui, Ui9CText), ZP, f, a2);
		}
	}

	/* content card */
	ui9_roundrect(&ui, rcontent, ui.theme.radius, ui9img(&ui, Ui9CSurface));
	border(screen, rcontent, 1, ui9img(&ui, Ui9CBorder), ZP);

	/* rows */
	draw_row(row1, "Start panel", "9de-panel on login");
	draw_row(row2, "Start demo",  "ui9demo on login");
	draw_row(row3, "Alpha / blend", "preview overlay opacity");
	draw_row(row4, "Text field", "focus + caret");
	draw_row(row5, "Mode", "layout preset");

	draw_checkbox(chk1, chkpanel);
	draw_checkbox(chk2, chkdemo);

	ui9_slider_draw(&ui, rslider, alpha_v);

	/* alpha preview */
	draw_checker(rpreview);
	draw_alpha_overlay(insetrect(rpreview, 3), alpha_v);

	ui9_textfield_draw(&ui, rtext, textbuf, textn, focused, "type here…");

	{
		char *labs[3] = { "flat", "debug", "dev" };
		ui9_segment3_draw(&ui, rseg, labs, segsel);
	}

	/* output area */
	{
		Rectangle box = rout;
		ui9_roundrect(&ui, box, ui.theme.radius, ui9img(&ui, Ui9CSurface));
		border(screen, box, 1, ui9img(&ui, Ui9CBorder), ZP);

		snprint(buf, sizeof buf, "ui_style=%s   ui_alpha=%d", getenv("ui_style")?getenv("ui_style"):"terminal", alpha_v);
		string(screen, Pt(box.min.x+10, box.min.y+10), ui9img(&ui, Ui9CText), ZP, f, "Output");
		string(screen, Pt(box.min.x+10, box.min.y+28), ui9img(&ui, Ui9CMuted), ZP, f, buf);

		snprint(buf, sizeof buf, "start_panel=%d start_demo=%d seg=%d focused=%d", chkpanel, chkdemo, segsel, focused);
		string(screen, Pt(box.min.x+10, box.min.y+46), ui9img(&ui, Ui9CMuted), ZP, f, buf);

		string(screen, Pt(box.min.x+10, box.max.y-22), ui9img(&ui, Ui9CMuted), ZP, f, "tip: press 'q' to quit");
	}

	flushimage(display, 1);
}

static int
pressed(Mouse m, int *ob)
{
	int p = (m.buttons & 1) && !(*ob & 1);
	*ob = m.buttons;
	return p;
}

static void
onmouse(Mouse m)
{
	static int ob;
	static int dragging;

	if(m.buttons & 1){
		/* drag slider */
		if(ptinrect(m.xy, rslider) || dragging){
			alpha_v = ui9_slider_value(rslider, m.xy);
			dragging = 1;
			redraw();
			return;
		}
	}else{
		dragging = 0;
	}

	if(!pressed(m, &ob))
		return;

	/* nav select */
	{
		int i;
		for(i=0; i<4; i++){
			if(ptinrect(m.xy, navitem[i])){
				navsel = i;
				redraw();
				return;
			}
		}
	}

	/* checkboxes */
	if(ptinrect(m.xy, chk1)){
		chkpanel = !chkpanel;
		redraw();
		return;
	}
	if(ptinrect(m.xy, chk2)){
		chkdemo = !chkdemo;
		redraw();
		return;
	}

	/* segment */
	{
		int hit = ui9_segment3_hit(rseg, m.xy);
		if(hit >= 0){
			segsel = hit;
			redraw();
			return;
		}
	}

	/* focus */
	focused = ptinrect(m.xy, rtext);
	redraw();
}

static void
text_add(Rune r)
{
	if(textn >= (int)(nelem(textbuf)-1))
		return;
	textbuf[textn++] = r;
	textbuf[textn] = 0;
}

static void
text_bs(void)
{
	if(textn <= 0)
		return;
	textn--;
	textbuf[textn] = 0;
}

static void
onkey(Rune r)
{
	if(r == 'q' || r == 'Q')
		exits(nil);

	if(!focused)
		return;

	if(r == '\b' || r == 0x7f){
		text_bs();
		redraw();
		return;
	}

	if(r == '\n' || r == '\r')
		return;

	if(r >= 0x20){
		text_add(r);
		redraw();
	}
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND

	if(initdraw(0, 0, "ui9demo") < 0)
		sysfatal("initdraw: %r");

	ui9init(&ui, display, font);
	ui9applyenv(&ui);
	ui9setdst(&ui, screen);

	einit(Emouse|Ekeyboard);

	redraw();

	for(;;){
		Event e;
		switch(event(&e)){
		case Emouse:
			onmouse(e.mouse);
			break;
		case Ekeyboard:
			onkey(e.kbdc);
			break;
		}
	}
}
