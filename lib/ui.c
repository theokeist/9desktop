#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../include/9deui/9deui.h"


static int
ishex(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int
hexval(int c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if(c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return 0;
}

/* Parse "#RRGGBB" / "RRGGBB" / "0xRRGGBB" -> RGB24 ulong. Returns 1 on success. */
static int
parsehexrgb(char *s, ulong *out)
{
	ulong v;
	int n, i;

	if(s == nil) return 0;
	while(*s == ' ' || *s == '\t') s++;

	if(s[0] == '#') s++;
	if(s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

	n = strlen(s);
	if(n < 6) return 0;

	for(i=0; i<6; i++)
		if(!ishex(s[i])) return 0;

	v = 0;
	for(i=0; i<6; i++)
		v = (v<<4) | (ulong)hexval(s[i]);

	*out = v;
	return 1;
}

static int
clampi(int v, int lo, int hi)
{
	if(v < lo) return lo;
	if(v > hi) return hi;
	return v;
}

static void
freeimg(Image **p)
{
	if(*p != nil){
		freeimage(*p);
		*p = nil;
	}
}

static Image*
mk1x1(Display *d, int chan, ulong col)
{
	return allocimage(d, Rect(0,0,1,1), chan, 1, col);
}

/* Build theme tokens as 1x1 images. */
static void
ui9rebuild(Ui9 *ui)
{
	Ui9Theme *t = &ui->theme;

	for(int i=0; i<Ui9CCount; i++)
		freeimg(&ui->img[i]);

	/* Base tokens */
	ui->img[Ui9CBg]       = mk1x1(ui->d, RGB24, t->bgrgb);
	ui->img[Ui9CSurface]  = mk1x1(ui->d, RGBA32,
		(t->style == Ui9StyleGlass) ? setalpha(t->surfacergb, (uchar)t->alpha) : setalpha(t->surfacergb, 255));
	ui->img[Ui9CSurface2] = mk1x1(ui->d, RGBA32,
		(t->style == Ui9StyleGlass) ? setalpha(t->surface2rgb, (uchar)clampi(t->alpha + 25, 0, 255)) : setalpha(t->surface2rgb, 255));

	ui->img[Ui9CText]   = mk1x1(ui->d, RGB24, t->textrgb);
	ui->img[Ui9CMuted]  = mk1x1(ui->d, RGB24, t->mutedrgb);

	/* Border + shadow are RGBA32 to allow style-controlled transparency */
	ui->img[Ui9CBorder] = mk1x1(ui->d, RGBA32, setalpha(t->borderrgb, (uchar)t->bordera));
	ui->img[Ui9CShadow] = mk1x1(ui->d, RGBA32, setalpha(DBlack, (uchar)t->shadowa));

	ui->img[Ui9CAccent]  = mk1x1(ui->d, RGBA32, setalpha(t->accentrgb, 255));
	ui->img[Ui9CAccent2] = mk1x1(ui->d, RGBA32, setalpha(t->accentrgb, 40));

	ui->img[Ui9CTopbarBg]   = mk1x1(ui->d, RGB24, t->topbgrgb);
	ui->img[Ui9CTopbarText] = mk1x1(ui->d, RGB24, t->toptextrgb);

	for(int i=0; i<Ui9CCount; i++)
		if(ui->img[i] == nil)
			sysfatal("ui9: theme alloc failed: %r");
}

void
ui9init(Ui9 *ui, Display *d, Font *font)
{
	memset(ui, 0, sizeof *ui);
	ui->d = d;
	ui->font = font;
	ui->dst = screen; /* default target */

	ui9theme_default(&ui->theme);
	ui9rebuild(ui);
}

void
ui9free(Ui9 *ui)
{
	for(int i=0; i<Ui9CCount; i++)
		freeimg(&ui->img[i]);
}

void
ui9setdst(Ui9 *ui, Image *dst)
{
	ui->dst = dst;
}

void
ui9setfont(Ui9 *ui, Font *font)
{
	ui->font = font;
}

void
ui9setalpha(Ui9 *ui, int alpha)
{
	ui->theme.alpha = clampi(alpha, 0, 255);
	ui9rebuild(ui);
}

void
ui9setstyle(Ui9 *ui, int style)
{
	Ui9Theme nt;
	ui9theme_style(&nt, style);

	/* preserve user alpha if they set it */
	nt.alpha = ui->theme.alpha;

	ui->theme = nt;
	ui9rebuild(ui);
}

void
ui9setaccent(Ui9 *ui, ulong rgb24)
{
	ui->theme.accentrgb = rgb24;
	ui9rebuild(ui);
}

void
ui9settopbar(Ui9 *ui, ulong bgrgb, ulong textrgb)
{
	ui->theme.topbgrgb = bgrgb;
	ui->theme.toptextrgb = textrgb;
	ui9rebuild(ui);
}

void
ui9setbordera(Ui9 *ui, int alpha)
{
	ui->theme.bordera = clampi(alpha, 0, 255);
	ui9rebuild(ui);
}

void
ui9setshadowa(Ui9 *ui, int alpha)
{
	ui->theme.shadowa = clampi(alpha, 0, 255);
	ui9rebuild(ui);
}

void
ui9setradius(Ui9 *ui, int radius)
{
	ui->theme.radius = clampi(radius, 0, 24);
}


void
ui9applyenv(Ui9 *ui)
{
	char *s;

	s = getenv("ui_style");
	if(s != nil)
		ui9setstyle(ui, ui9style_fromname(s));

	s = getenv("ui_alpha");
	if(s != nil)
		ui9setalpha(ui, atoi(s));

	/* optional small overrides: still "preset-based", not infinite theming */
	{
		ulong rgb;
		s = getenv("ui_accent");
		if(s != nil && parsehexrgb(s, &rgb))
			ui9setaccent(ui, rgb);
		s = getenv("ui_topbg");
		if(s != nil && parsehexrgb(s, &rgb))
			ui->theme.topbgrgb = rgb;
		s = getenv("ui_toptext");
		if(s != nil && parsehexrgb(s, &rgb))
			ui->theme.toptextrgb = rgb;
		/* if any topbar override happened, rebuild once */
		if(getenv("ui_topbg") != nil || getenv("ui_toptext") != nil)
			ui9rebuild(ui);
	}

	s = getenv("ui_border_alpha");
	if(s != nil)
		ui9setbordera(ui, atoi(s));
	s = getenv("ui_shadow_alpha");
	if(s != nil)
		ui9setshadowa(ui, atoi(s));
	s = getenv("ui_radius");
	if(s != nil)
		ui9setradius(ui, atoi(s));
}

Image*
ui9img(Ui9 *ui, int role)
{
	if(role < 0 || role >= Ui9CCount)
		return nil;
	return ui->img[role];
}
