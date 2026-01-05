#ifndef _9DEUI_THEME_H_
#define _9DEUI_THEME_H_

typedef struct Ui9Theme Ui9Theme;
typedef struct Ui9 Ui9;

/*
 * Theme color roles.
 *
 * NOTE: keep this list short. 9DE is not “infinite theming”; it’s a small set
 * of predefined styles with strict tokens, like macOS “appearance” presets.
 */
enum {
	Ui9CBg = 0,        /* app background */
	Ui9CSurface,       /* main surface */
	Ui9CSurface2,      /* secondary surface */
	Ui9CText,          /* primary text */
	Ui9CMuted,         /* secondary text */
	Ui9CBorder,        /* borders + separators */
	Ui9CShadow,        /* shadows + text shadow (can be fully transparent) */
	Ui9CAccent,        /* primary accent */
	Ui9CAccent2,       /* accent-tinted highlight */
	Ui9CTopbarBg,      /* 9de-panel background */
	Ui9CTopbarText,    /* 9de-panel text */
	Ui9CCount,
};

enum {
	Ui9StyleTerminal = 0,   /* default: terminal-chic */
	Ui9StyleDark     = 1,   /* dark preset */
	Ui9StyleGlass    = 2,   /* experimental “glass” preset */
};

struct Ui9Theme {
	/* geometry */
	int pad;        /* default padding */
	int radius;     /* default corner radius */

	/* style id */
	int style;

	/* alpha controls (mostly for Ui9StyleGlass) */
	int alpha;      /* 0..255 “glass strength” */
	int bordera;    /* border alpha */
	int shadowa;    /* shadow alpha */

	/* RGB24 base colors (alpha applied during image build) */
	ulong bgrgb;
	ulong surfacergb;
	ulong surface2rgb;
	ulong textrgb;
	ulong mutedrgb;
	ulong borderrgb;
	ulong topbgrgb;
	ulong toptextrgb;
	ulong accentrgb;
};

struct Ui9 {
	/* public, but treat as read-only after init */
	Display *d;
	Font *font;
	Image *dst;         /* draw target (usually screen) */
	Ui9Theme theme;

	/* cached 1x1 theme images by role */
	Image *img[Ui9CCount];
	/* input snapshot (optional) */
	Mouse m;
	Rune  k;

	/* focus/capture ids */
	ulong focusid;
	ulong captureid;
};

/* theme setup */
void  ui9theme_default(Ui9Theme *t);
void  ui9theme_style(Ui9Theme *t, int style);
int   ui9style_fromname(char *s);

/* ui object */
void  ui9init(Ui9 *ui, Display *d, Font *font);
void  ui9free(Ui9 *ui);

void  ui9setdst(Ui9 *ui, Image *dst);
void  ui9setfont(Ui9 *ui, Font *font);
void  ui9setalpha(Ui9 *ui, int alpha);
void  ui9setstyle(Ui9 *ui, int style);
void  ui9setaccent(Ui9 *ui, ulong rgb24);
void  ui9settopbar(Ui9 *ui, ulong bgrgb, ulong textrgb);
void  ui9setbordera(Ui9 *ui, int alpha);
void  ui9setshadowa(Ui9 *ui, int alpha);
void  ui9setradius(Ui9 *ui, int radius);

/* apply common env vars:
 *   ui_style=terminal|dark|glass
 *   ui_alpha=0..255
 *   ui_accent=#RRGGBB | RRGGBB
 *   ui_topbg=#RRGGBB | RRGGBB
 *   ui_toptext=#RRGGBB | RRGGBB
 *   ui_border_alpha=0..255
 *   ui_shadow_alpha=0..255
 *   ui_radius=0..24
 */
void  ui9applyenv(Ui9 *ui);

/* theme color as 1x1 image */
Image* ui9img(Ui9 *ui, int role);

#endif
