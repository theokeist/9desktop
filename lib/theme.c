#include <u.h>
#include <libc.h>
#include <draw.h>
#include <9deui/9deui.h>

static ulong
rgb24(int r, int g, int b)
{
	return (r<<16) | (g<<8) | b;
}

void
ui9theme_style(Ui9Theme *t, int style)
{
	memset(t, 0, sizeof *t);
	t->style = style;

	/* defaults shared across styles */
	t->pad = 12;
	t->radius = 4;
	t->alpha = 185;
	t->bordera = 255;
	t->shadowa = 0;

	switch(style){
	default:
	case Ui9StyleTerminal:
		/* “terminal-chic”: paper background, crisp borders, minimal accent */
		t->bgrgb       = rgb24(244,244,244);
		t->surfacergb  = rgb24(255,255,255);
		t->surface2rgb = rgb24(250,250,250);
		t->textrgb     = rgb24(25,25,25);
		t->mutedrgb    = rgb24(110,110,110);
		t->borderrgb   = rgb24(205,205,205);
		t->topbgrgb    = rgb24(18,18,18);
		t->toptextrgb  = rgb24(230,230,230);
		t->accentrgb   = rgb24(60,130,255);

		t->shadowa     = 0;   /* disable text shadow for a crisp look */
		break;

	case Ui9StyleDark:
		t->bgrgb       = rgb24(24,24,24);
		t->surfacergb  = rgb24(30,30,30);
		t->surface2rgb = rgb24(20,20,20);
		t->textrgb     = rgb24(235,235,235);
		t->mutedrgb    = rgb24(160,160,160);
		t->borderrgb   = rgb24(70,70,70);
		t->topbgrgb    = rgb24(12,12,12);
		t->toptextrgb  = rgb24(235,235,235);
		t->accentrgb   = rgb24(60,130,255);

		t->shadowa     = 90;
		break;

	case Ui9StyleGlass:
		/* experimental: translucent surfaces. keep controlled. */
		t->bgrgb       = rgb24(18,18,22);
		t->surfacergb  = rgb24(255,255,255);
		t->surface2rgb = rgb24(255,255,255);
		t->textrgb     = rgb24(240,240,245);
		t->mutedrgb    = rgb24(165,165,175);
		t->borderrgb   = rgb24(255,255,255);
		t->topbgrgb    = rgb24(12,12,14);
		t->toptextrgb  = rgb24(235,235,240);
		t->accentrgb   = rgb24(60,130,255);

		t->alpha = 185;
		t->bordera = 55;
		t->shadowa = 170;
		t->radius = 6;
		break;
	}
}

void
ui9theme_default(Ui9Theme *t)
{
	/* default style locked early: terminal-chic */
	ui9theme_style(t, Ui9StyleTerminal);
}

int
ui9style_fromname(char *s)
{
	if(s == nil) return Ui9StyleTerminal;
	if(strcmp(s, "terminal") == 0) return Ui9StyleTerminal;
	if(strcmp(s, "dark") == 0) return Ui9StyleDark;
	if(strcmp(s, "glass") == 0) return Ui9StyleGlass;
	return Ui9StyleTerminal;
}
