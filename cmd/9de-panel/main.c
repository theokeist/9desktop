#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <bio.h>

#include <9deui/9deui.h>

/*
 * 9de-panel v6: gradients + real "appearance knobs" (config.rc), still engineered.
 *
 * What you can tweak (no recompiles):
 *   $home/lib/9de/config.rc
 *
 * Panel layout tokens:
 *   panel_pad, panel_gap, panel_chip, panel_ws_maxw, panel_win_maxw
 *
 * Appearance (preset-based):
 *   ui_style=terminal|dark|glass
 *   ui_alpha=0..255
 *   ui_accent=#RRGGBB
 *   ui_topbg=#RRGGBB
 *   ui_toptext=#RRGGBB
 *   ui_border_alpha=0..255
 *   ui_shadow_alpha=0..255
 *   ui_radius=0..24
 *   ui_font=/lib/font/...
 *
 * Gradient (this request):
 *   ui_topgrad=1/0
 *   ui_topgrad0=#RRGGBB   (optional override)
 *   ui_topgrad1=#RRGGBB   (optional override)
 *   ui_minigrad=1/0
 *   ui_minigrad0=#RRGGBB  (optional)
 *   ui_minigrad1=#RRGGBB  (optional)
 *
 * Defaults:
 *   gradients ON (subtle), chips-on-hover ON, rounded corners are small.
 *
 * Still OS-level:
 * - reads /dev/wsys/* for labels
 * - focuses by writing "current" to /dev/wsys/<id>/wctl
 * - best-effort mounts /srv/9de to /mnt/9de and watches /mnt/9de/events (in helper proc)
 */

enum { MaxMods = 24 };
enum { PanelHDefault = 34 };
enum { MiniHDefault  = 28 };
enum { MaxStr = 128 };
enum { MaxWins = 16 };
enum { MaxGrad = 256 };

typedef struct Panel Panel;
typedef struct Pmod Pmod;
typedef struct WinEnt WinEnt;

struct WinEnt {
	int id;
	int current;
	char label[MaxStr];
};

struct Panel {
	Ui9 ui;
	Image *dst;
	Image *buf;
	Rectangle r;

	/* heights */
	int baseh;
	int minih;
	int h;
	int expanded;

	/* panel layout tokens (changeable) */
	int pad;
	int gap;
	int chip;          /* 1=chips-on-hover */
	int ws_maxw;
	int win_maxw;

	/* runtime */
	int dirty;
	int ascii;
	Point mousexy;
	int mousebuttons;


	/* modules (rebuilt on reload) */
	Pmod *leftmods[MaxMods];
	Pmod *rightmods[MaxMods];
	int nleft;
	int nright;
	int need_reload;

	/* focus */
	int focus_menu;

	/* model */
	int de_up;
	char de_label[MaxStr];
	char net_label[MaxStr];
	char clock_label[MaxStr];
	char preset_label[MaxStr];
	int notif_count;

	/* window model */
	char ws_label[MaxStr];
	WinEnt wins[MaxWins];
	int nwins;
	int ws_hover;
	int ws_hover_id;

	/* module rect cache */
	Rectangle wsrect;

	/* appearance config (preset-based) */
	char ui_style_name[32];
	int ui_alpha;           /* -1 if unset */
	char ui_accent[32];
	char ui_topbg[32];
	char ui_toptext[32];
	int ui_border_alpha;    /* -1 if unset */
	int ui_shadow_alpha;    /* -1 if unset */
	int ui_radius;          /* -1 if unset */
	char ui_font_path[256];

	/* gradient config + cache */
	int ui_topgrad;         /* 1 on */
	char ui_topgrad0[32];
	char ui_topgrad1[32];
	int ui_minigrad;
	char ui_minigrad0[32];
	char ui_minigrad1[32];

	Image *topgrad[MaxGrad];
	int topgrad_n;
	ulong topgrad_c0, topgrad_c1;
	int topgrad_h;

	Image *minigrad[MaxGrad];
	int minigrad_n;
	ulong minigrad_c0, minigrad_c1;
	int minigrad_h;

	/* config */
	char panel_left_cfg[256];
	char panel_right_cfg[256];
	int enable_watch;
};

struct Pmod {
	char *name;
	int side;        /* 0 left, 1 right */
	int w;
	int clickable;

	void (*init)(Panel*, Pmod*);
	int  (*measure)(Panel*, Pmod*);
	void (*draw)(Panel*, Pmod*, Image*, Rectangle, int hover, int pressed);
	int  (*hit)(Panel*, Pmod*, Mouse*, Rectangle);

	Rectangle last;
};

/* ----------------- helpers ----------------- */

static char*
home(void)
{
	char *h = getenv("home");
	return h ? h : "/usr/glenda";
}

static void
spawnrc(char *cmd)
{
	int pid;
	pid = rfork(RFPROC|RFFDG|RFNOWAIT);
	if(pid == 0){
		execl("/bin/rc", "rc", "-c", cmd, nil);
		exits("exec rc");
	}
}

static void
ensurelogdir(void)
{
	char p[256];
	snprint(p, sizeof p, "%s/lib/9de/log", home());
	mkdir(p);
}

/* smooth: offscreen buffer */
static void
bufrealloc(Panel *p)
{
	Rectangle r;

	if(p->dst == nil)
		return;

	r = p->dst->r;
	if(p->buf != nil){
		if(Dx(p->buf->r) == Dx(r) && Dy(p->buf->r) == Dy(r))
			return;
		freeimage(p->buf);
		p->buf = nil;
	}

	p->buf = allocimage(display, Rect(0,0,Dx(r),Dy(r)), p->dst->chan, 0, 0x00000000);
}

/* ----------------- basic string helpers ----------------- */

static void
chomp(char *s)
{
	int n = strlen(s);
	while(n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
		s[--n] = 0;
}

static char*
sym(Panel *p, char *utf8, char *ascii)
{
	return p->ascii ? ascii : utf8;
}

/* ----------------- config parser ----------------- */

static int isws(int c){ return c==' ' || c=='\t' || c=='\r' || c=='\n'; }

static void
trim(char *s)
{
	char *p, *e;
	while(*s && isws(*s)) s++;
	if(s[0] != 0){
		p = s;
		while(isws(*p)) p++;
		if(p != s) memmove(s, p, strlen(p)+1);
	}
	e = s + strlen(s);
	while(e > s && isws(e[-1])) e--;
	*e = 0;
}

static void
stripquotes(char *s)
{
	int n = strlen(s);
	if(n >= 2 && ((s[0]=='"' && s[n-1]=='"') || (s[0]=='\'' && s[n-1]=='\''))){
		memmove(s, s+1, n-2);
		s[n-2] = 0;
	}
}

static void
normalize_list(char *v)
{
	trim(v);
	stripquotes(v);
	trim(v);

	if(v[0] == '('){
		char *e = v + strlen(v) - 1;
		if(e > v && *e == ')'){
			memmove(v, v+1, (e - (v+1)));
			v[e - (v+1)] = 0;
			trim(v);
		}
	}
}

/* ----------------- color helpers (hex + rgb ops) ----------------- */

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
	if(n != 6) return 0;
	for(i=0; i<n; i++)
		if(!ishex(s[i])) return 0;

	v = 0;
	for(i=0; i<6; i++){
		v = (v<<4) | hexval(s[i]);
	}
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

static ulong
rgb_add(ulong rgb, int d)
{
	int r = clampi((rgb>>16)&0xff, 0, 255);
	int g = clampi((rgb>>8)&0xff, 0, 255);
	int b = clampi(rgb&0xff, 0, 255);
	r = clampi(r + d, 0, 255);
	g = clampi(g + d, 0, 255);
	b = clampi(b + d, 0, 255);
	return ((r<<16) | (g<<8) | b);
}

static ulong
rgb_lerp(ulong a, ulong b, int t, int tmax)
{
	int ar=(a>>16)&0xff, ag=(a>>8)&0xff, ab=a&0xff;
	int br=(b>>16)&0xff, bg=(b>>8)&0xff, bb=b&0xff;

	int r = ar + ((br - ar) * t) / tmax;
	int g = ag + ((bg - ag) * t) / tmax;
	int bl = ab + ((bb - ab) * t) / tmax;

	return ((r<<16) | (g<<8) | bl);
}

/* ----------------- gradient cache ----------------- */

static void
freegrad(Image **a, int n)
{
	int i;
	for(i=0; i<n; i++){
		if(a[i] != nil){
			freeimage(a[i]);
			a[i] = nil;
		}
	}
}

static Image*
mk1x1rgb24(ulong rgb)
{
	return allocimage(display, Rect(0,0,1,1), RGB24, 1, rgb);
}

static void
ensurevgrad(Image **a, int *pn, ulong *pc0, ulong *pc1, int *ph, int h, ulong c0, ulong c1)
{
	int i, n;

	if(h <= 0) return;
	if(h > MaxGrad) h = MaxGrad;

	if(*ph == h && *pc0 == c0 && *pc1 == c1 && *pn == h)
		return;

	freegrad(a, *pn);
	*pn = 0;

	n = h;
	for(i=0; i<n; i++){
		int t = (n <= 1) ? 0 : (i * 255) / (n-1);
		ulong c = rgb_lerp(c0, c1, t, 255);
		a[i] = mk1x1rgb24(c);
		if(a[i] == nil)
			break;
		(*pn)++;
	}

	*pc0 = c0;
	*pc1 = c1;
	*ph = h;
}

static void
drawvgrad(Image *dst, Rectangle r, Image **a, int n)
{
	int y, h;

	h = Dy(r);
	if(h > n) h = n;
	for(y=0; y<h; y++){
		Rectangle rr = Rect(r.min.x, r.min.y + y, r.max.x, r.min.y + y + 1);
		draw(dst, rr, a[y], nil, ZP);
	}
}

/* ----------------- load config ----------------- */

static void
loadconfig(Panel *p)
{
	char path[256];
	Biobuf *b;
	char *ln;
	int fd;

	snprint(path, sizeof path, "%s/lib/9de/config.rc", home());
	fd = open(path, OREAD);
	if(fd < 0)
		return;

	b = Bfdopen(fd, OREAD);
	if(b == nil){
		close(fd);
		return;
	}

	for(;;){
		ln = Brdline(b, '\n');
		if(ln == nil)
			break;
		ln[Blinelen(b)-1] = 0;

		{
			char *c = strchr(ln, '#');
			if(c) *c = 0;
		}
		trim(ln);
		if(ln[0] == 0)
			continue;

		{
			char *eq = strchr(ln, '=');
			char key[64], val[256];
			int klen;

			if(eq == nil) continue;
			klen = eq - ln;
			if(klen <= 0 || klen >= (int)sizeof key) continue;

			memmove(key, ln, klen);
			key[klen] = 0;
			snprint(val, sizeof val, "%s", eq+1);

			trim(key);
			trim(val);
			normalize_list(val);

			/* panel layout */
			if(strcmp(key, "panel_left") == 0){
				strecpy(p->panel_left_cfg, p->panel_left_cfg+sizeof p->panel_left_cfg, val);
			}else if(strcmp(key, "panel_right") == 0){
				strecpy(p->panel_right_cfg, p->panel_right_cfg+sizeof p->panel_right_cfg, val);
			}else if(strcmp(key, "panel_height") == 0){
				p->baseh = atoi(val);
				if(p->baseh <= 16) p->baseh = PanelHDefault;
			}else if(strcmp(key, "panel_minih") == 0){
				p->minih = atoi(val);
				if(p->minih < 16) p->minih = MiniHDefault;
			}else if(strcmp(key, "panel_ascii") == 0){
				p->ascii = atoi(val) != 0;
			}else if(strcmp(key, "panel_watch") == 0){
				p->enable_watch = atoi(val) != 0;
			}else if(strcmp(key, "panel_pad") == 0){
				p->pad = atoi(val);
			}else if(strcmp(key, "panel_gap") == 0){
				p->gap = atoi(val);
			}else if(strcmp(key, "panel_chip") == 0){
				p->chip = atoi(val) != 0;
			}else if(strcmp(key, "panel_ws_maxw") == 0){
				p->ws_maxw = atoi(val);
			}else if(strcmp(key, "panel_win_maxw") == 0){
				p->win_maxw = atoi(val);
			}

			/* appearance */
			else if(strcmp(key, "ui_style") == 0){
				strecpy(p->ui_style_name, p->ui_style_name+sizeof p->ui_style_name, val);
			}else if(strcmp(key, "ui_alpha") == 0){
				p->ui_alpha = atoi(val);
			}else if(strcmp(key, "ui_accent") == 0){
				strecpy(p->ui_accent, p->ui_accent+sizeof p->ui_accent, val);
			}else if(strcmp(key, "ui_topbg") == 0){
				strecpy(p->ui_topbg, p->ui_topbg+sizeof p->ui_topbg, val);
			}else if(strcmp(key, "ui_toptext") == 0){
				strecpy(p->ui_toptext, p->ui_toptext+sizeof p->ui_toptext, val);
			}else if(strcmp(key, "ui_border_alpha") == 0){
				p->ui_border_alpha = atoi(val);
			}else if(strcmp(key, "ui_shadow_alpha") == 0){
				p->ui_shadow_alpha = atoi(val);
			}else if(strcmp(key, "ui_radius") == 0){
				p->ui_radius = atoi(val);
			}else if(strcmp(key, "ui_font") == 0){
				strecpy(p->ui_font_path, p->ui_font_path+sizeof p->ui_font_path, val);
			}

			/* gradients */
			else if(strcmp(key, "ui_topgrad") == 0){
				p->ui_topgrad = atoi(val) != 0;
			}else if(strcmp(key, "ui_topgrad0") == 0){
				strecpy(p->ui_topgrad0, p->ui_topgrad0+sizeof p->ui_topgrad0, val);
			}else if(strcmp(key, "ui_topgrad1") == 0){
				strecpy(p->ui_topgrad1, p->ui_topgrad1+sizeof p->ui_topgrad1, val);
			}else if(strcmp(key, "ui_minigrad") == 0){
				p->ui_minigrad = atoi(val) != 0;
			}else if(strcmp(key, "ui_minigrad0") == 0){
				strecpy(p->ui_minigrad0, p->ui_minigrad0+sizeof p->ui_minigrad0, val);
			}else if(strcmp(key, "ui_minigrad1") == 0){
				strecpy(p->ui_minigrad1, p->ui_minigrad1+sizeof p->ui_minigrad1, val);
			}
		}
	}

	Bterm(b);
}


/* ----------------- reload helpers ----------------- */

static void
setcfgdefaults(Panel *p)
{
	/* base defaults (keep runtime state elsewhere) */
	p->baseh = PanelHDefault;
	p->minih = MiniHDefault;
	p->h = p->baseh;

	/* layout tokens */
	p->pad = 10;
	p->gap = 8;
	p->chip = 1;
	p->ws_maxw = 140;
	p->win_maxw = 240;

	p->ascii = 0;
	p->enable_watch = 1;

	/* appearance defaults (unset means "theme default") */
	p->ui_style_name[0] = 0;
	p->ui_alpha = -1;
	p->ui_accent[0] = 0;
	p->ui_topbg[0] = 0;
	p->ui_toptext[0] = 0;
	p->ui_border_alpha = -1;
	p->ui_shadow_alpha = -1;
	p->ui_radius = -1;
	p->ui_font_path[0] = 0;

	/* gradients */
	p->ui_topgrad = 0;
	strecpy(p->ui_topgrad0, p->ui_topgrad0+sizeof p->ui_topgrad0, "#0c0c0e");
	strecpy(p->ui_topgrad1, p->ui_topgrad1+sizeof p->ui_topgrad1, "#1a1a20");

	p->ui_minigrad = 0;
	strecpy(p->ui_minigrad0, p->ui_minigrad0+sizeof p->ui_minigrad0, "#101014");
	strecpy(p->ui_minigrad1, p->ui_minigrad1+sizeof p->ui_minigrad1, "#18181f");

	/* config strings */
	strecpy(p->panel_left_cfg, p->panel_left_cfg+sizeof p->panel_left_cfg, "menu ws");
	strecpy(p->panel_right_cfg, p->panel_right_cfg+sizeof p->panel_right_cfg, "preset de net clock notif");
}

static void
applyappearance(Panel *p, Font *fnt)
{
	/* optional font override */
	if(p->ui_font_path[0] != 0){
		Font *nf = openfont(display, p->ui_font_path);
		if(nf != nil)
			fnt = nf;
	}

	/* ui already inited; update theme tokens */
	if(p->ui_style_name[0] != 0)
		ui9setstyle(&p->ui, ui9style_fromname(p->ui_style_name));
	if(p->ui_alpha >= 0)
		ui9setalpha(&p->ui, p->ui_alpha);

	if(p->ui_accent[0] != 0){
		ulong rgb;
		if(parsehexrgb(p->ui_accent, &rgb))
			ui9setaccent(&p->ui, rgb);
	}
	if(p->ui_topbg[0] != 0 || p->ui_toptext[0] != 0){
		ulong bg, tx;
		int okbg=0, oktx=0;
		okbg = (p->ui_topbg[0]!=0) && parsehexrgb(p->ui_topbg, &bg);
		oktx = (p->ui_toptext[0]!=0) && parsehexrgb(p->ui_toptext, &tx);
		if(okbg || oktx){
			if(!okbg) bg = p->ui.theme.topbgrgb;
			if(!oktx) tx = p->ui.theme.toptextrgb;
			ui9settopbar(&p->ui, bg, tx);
		}
	}
	if(p->ui_border_alpha >= 0) ui9setbordera(&p->ui, p->ui_border_alpha);
	if(p->ui_shadow_alpha >= 0) ui9setshadowa(&p->ui, p->ui_shadow_alpha);
	if(p->ui_radius >= 0) ui9setradius(&p->ui, p->ui_radius);

	ui9applyenv(&p->ui);
	ui9setdst(&p->ui, p->dst);
}

static void
initmods(Panel *p)
{
	char *ltmp, *rtmp;
	char *lw[MaxMods], *rw[MaxMods];
	int i;

	/* clear */
	for(i=0; i<MaxMods; i++){
		p->leftmods[i] = nil;
		p->rightmods[i] = nil;
	}
	p->nleft = 0;
	p->nright = 0;

	ltmp = strdup(p->panel_left_cfg);
	rtmp = strdup(p->panel_right_cfg);
	if(ltmp == nil || rtmp == nil){
		free(ltmp); free(rtmp);
		return;
	}

	p->nleft = splitwords(ltmp, lw, MaxMods);
	p->nright = splitwords(rtmp, rw, MaxMods);

	for(i=0; i<p->nleft; i++){
		Pmod *m;
		if(findmod(lw[i], &m)){
			p->leftmods[i] = m;
			if(m->init) m->init(p, m);
		}
	}
	for(i=0; i<p->nright; i++){
		Pmod *m;
		if(findmod(rw[i], &m)){
			p->rightmods[i] = m;
			if(m->init) m->init(p, m);
		}
	}

	free(ltmp);
	free(rtmp);
}

static void
reloadpanel(Panel *p, Font *fnt)
{
	/* reset config-related fields, then reload from file + env, then rebuild modules */
	setcfgdefaults(p);
	loadconfig(p);

	/* env overrides (same behavior as boot) */
	if(getenv("panel_left") != nil)   strecpy(p->panel_left_cfg, p->panel_left_cfg+sizeof p->panel_left_cfg, getenv("panel_left"));
	if(getenv("panel_right") != nil)  strecpy(p->panel_right_cfg, p->panel_right_cfg+sizeof p->panel_right_cfg, getenv("panel_right"));
	if(getenv("panel_height") != nil) p->baseh = atoi(getenv("panel_height"));
	if(getenv("panel_minih") != nil)  p->minih = atoi(getenv("panel_minih"));
	if(getenv("panel_ascii") != nil)  p->ascii = 1;

	if(getenv("ui_style") != nil)     strecpy(p->ui_style_name, p->ui_style_name+sizeof p->ui_style_name, getenv("ui_style"));
	if(getenv("ui_alpha") != nil)     p->ui_alpha = atoi(getenv("ui_alpha"));
	if(getenv("ui_accent") != nil)    strecpy(p->ui_accent, p->ui_accent+sizeof p->ui_accent, getenv("ui_accent"));
	if(getenv("ui_topbg") != nil)     strecpy(p->ui_topbg, p->ui_topbg+sizeof p->ui_topbg, getenv("ui_topbg"));
	if(getenv("ui_toptext") != nil)   strecpy(p->ui_toptext, p->ui_toptext+sizeof p->ui_toptext, getenv("ui_toptext"));
	if(getenv("ui_border_alpha") != nil) p->ui_border_alpha = atoi(getenv("ui_border_alpha"));
	if(getenv("ui_shadow_alpha") != nil) p->ui_shadow_alpha = atoi(getenv("ui_shadow_alpha"));
	if(getenv("ui_radius") != nil)    p->ui_radius = atoi(getenv("ui_radius"));
	if(getenv("ui_font") != nil)      strecpy(p->ui_font_path, p->ui_font_path+sizeof p->ui_font_path, getenv("ui_font"));

	if(getenv("ui_topgrad") != nil)   p->ui_topgrad = atoi(getenv("ui_topgrad")) != 0;
	if(getenv("ui_topgrad0") != nil)  strecpy(p->ui_topgrad0, p->ui_topgrad0+sizeof p->ui_topgrad0, getenv("ui_topgrad0"));
	if(getenv("ui_topgrad1") != nil)  strecpy(p->ui_topgrad1, p->ui_topgrad1+sizeof p->ui_topgrad1, getenv("ui_topgrad1"));

	if(getenv("ui_minigrad") != nil)  p->ui_minigrad = atoi(getenv("ui_minigrad")) != 0;
	if(getenv("ui_minigrad0") != nil) strecpy(p->ui_minigrad0, p->ui_minigrad0+sizeof p->ui_minigrad0, getenv("ui_minigrad0"));
	if(getenv("ui_minigrad1") != nil) strecpy(p->ui_minigrad1, p->ui_minigrad1+sizeof p->ui_minigrad1, getenv("ui_minigrad1"));

	if(p->baseh <= 16) p->baseh = PanelHDefault;
	if(p->minih < 16)  p->minih = MiniHDefault;
	p->h = p->baseh;

	applyappearance(p, fnt);
	initmods(p);
	setpanelheight(p, p->h);
	markdirty(p);
}

/* ----------------- model formatters ----------------- */

static void
fmtclock(Panel *p)
{
	Tm *t = localtime(time(0));
	snprint(p->clock_label, sizeof p->clock_label, "%02d:%02d:%02d", t->hour, t->min, t->sec);
}

static void
fmtnet(Panel *p)
{
	char buf[1024];
	char *ip, *s, *q;
	int fd, n;

	ip = nil;
	fd = open("/net/ipifc/0/status", OREAD);
	if(fd < 0)
		fd = open("/net/ipifc/1/status", OREAD);
	if(fd >= 0){
		n = read(fd, buf, sizeof buf-1);
		close(fd);
		if(n > 0){
			buf[n] = 0;
			s = strstr(buf, "ip=");
			if(s != nil){
				s += 3;
				q = s;
				while(*q && *q!=' ' && *q!='\n' && (q-s) < 63) q++;
				ip = smprint("%.*s", (int)(q-s), s);
			}
		}
	}

	if(ip != nil){
		snprint(p->net_label, sizeof p->net_label, "%s", ip);
		free(ip);
	}else{
		snprint(p->net_label, sizeof p->net_label, "net");
	}
}

static void
fmtde(Panel *p)
{
	if(access("/srv/9de", AEXIST) == 0){
		p->de_up = 1;
		snprint(p->de_label, sizeof p->de_label, "de: ok");
	}else{
		p->de_up = 0;
		snprint(p->de_label, sizeof p->de_label, "de: down");
	}
}

static void
fmtpreset(Panel *p)
{
	if(p->preset_label[0] == 0)
		snprint(p->preset_label, sizeof p->preset_label, "style: terminal");
}

/* text fitting */
static void
ellipsize(Panel *p, char *dst, int ndst, char *src, int maxpx, Font *f)
{
	char tmp[MaxStr];
	int n;

	if(src == nil){
		dst[0] = 0;
		return;
	}

	strecpy(tmp, tmp+sizeof tmp, src);

	if(stringwidth(f, tmp) <= maxpx){
		strecpy(dst, dst+ndst, tmp);
		return;
	}

	for(n=strlen(tmp); n>0; n--){
		tmp[n] = 0;
		if(p->ascii)
			snprint(dst, ndst, "%s...", tmp);
		else
			snprint(dst, ndst, "%s…", tmp);
		if(stringwidth(f, dst) <= maxpx)
			return;
	}

	if(p->ascii)
		strecpy(dst, dst+ndst, "...");
	else
		strecpy(dst, dst+ndst, "…");
}

/* ----------------- rio window model ----------------- */

static int
readfile(char *path, char *buf, int nbuf)
{
	int fd, n;
	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = read(fd, buf, nbuf-1);
	close(fd);
	if(n < 0)
		return -1;
	buf[n] = 0;
	return n;
}

/* wctl: "minx miny maxx maxy pid current" (format can vary; we only need coords + current) */
static int
parsewctl(char *s, int *minx, int *miny, int *maxx, int *maxy, int *iscurrent)
{
	char *f[16];
	int nf;

	nf = getfields(s, f, 16, 1, " \t\r\n");
	if(nf < 6)
		return 0;

	*minx = atoi(f[0]);
	*miny = atoi(f[1]);
	*maxx = atoi(f[2]);
	*maxy = atoi(f[3]);
	*iscurrent = (strcmp(f[5], "current") == 0);
	return 1;
}

static void
updatewins(Panel *p)
{
	int fd, nd, i, n, curid;
	Dir *d;
	char path[256], buf[512];
	int minx, miny, maxx, maxy, iscur;

	p->nwins = 0;
	curid = -1;

	fd = open("/dev/wsys", OREAD);
	if(fd < 0)
		return;

	nd = dirreadall(fd, &d);
	close(fd);
	if(nd <= 0 || d == nil)
		return;

	for(i=0; i<nd && p->nwins < MaxWins; i++){
		if(d[i].qid.type & QTDIR){
			int id = atoi(d[i].name);
			if(id <= 0)
				continue;

			snprint(path, sizeof path, "/dev/wsys/%d/wctl", id);
			n = readfile(path, buf, sizeof buf);
			if(n < 0)
				continue;
			if(!parsewctl(buf, &minx, &miny, &maxx, &maxy, &iscur))
				continue;

			snprint(path, sizeof path, "/dev/wsys/%d/label", id);
			if(readfile(path, buf, sizeof buf) < 0)
				strecpy(buf, buf+sizeof buf, "window");
			chomp(buf);

			p->wins[p->nwins].id = id;
			p->wins[p->nwins].current = iscur;
			strecpy(p->wins[p->nwins].label, p->wins[p->nwins].label+sizeof p->wins[p->nwins].label, buf);

			if(iscur)
				curid = id;

			p->nwins++;
		}
	}

	free(d);

	if(curid >= 0){
		for(i=0; i<p->nwins; i++){
			if(p->wins[i].id == curid){
				Font *f = p->ui.font ? p->ui.font : font;
				ellipsize(p, p->ws_label, sizeof p->ws_label, p->wins[i].label, p->ws_maxw - 40, f);
				return;
			}
		}
	}

	if(p->ws_label[0] == 0)
		strecpy(p->ws_label, p->ws_label+sizeof p->ws_label, "ws");
}

static void
focuswin(int id)
{
	char path[128];
	int fd;

	snprint(path, sizeof path, "/dev/wsys/%d/wctl", id);
	fd = open(path, OWRITE);
	if(fd < 0)
		return;
	write(fd, "current", 7);
	close(fd);
}

static void
setpanelheight(Panel *p, int newh)
{
	int fd, n;
	char buf[256];
	int minx, miny, maxx, maxy, iscur;
	char cmd[128];

	fd = open("/dev/wctl", ORDWR);
	if(fd < 0)
		return;

	n = read(fd, buf, sizeof buf-1);
	if(n <= 0){
		close(fd);
		return;
	}
	buf[n] = 0;

	if(!parsewctl(buf, &minx, &miny, &maxx, &maxy, &iscur)){
		close(fd);
		return;
	}

	snprint(cmd, sizeof cmd, "resize -r %d %d %d %d", minx, miny, maxx, miny + newh);
	seek(fd, 0, 0);
	write(fd, cmd, strlen(cmd));
	close(fd);
}

/* ----------------- /srv watcher ----------------- */

static void
markdirty(void *arg)
{
	Panel *p = arg;
	p->dirty = 1;
}

static void
apply_status_snapshot(Panel *p)
{
	int fd, n;
	char buf[256];

	fd = open("/mnt/9de/status", OREAD);
	if(fd < 0) return;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0) return;
	buf[n] = 0;

	{
		char *s = strstr(buf, "preset ");
		if(s){
			s += 7;
			chomp(s);
			snprint(p->preset_label, sizeof p->preset_label, "style: %s", s);
		}
	}
}

static void
srvwatch(Panel *p)
{
	Biobuf *b;
	char *ln;
	int fd;

	if(access("/srv/9de", AEXIST) != 0){
		p->de_up = 0;
		snprint(p->de_label, sizeof p->de_label, "de: down");
		return;
	}

	mkdir("/mnt/9de");
	if(access("/mnt/9de/status", AEXIST) != 0)
		system("mount -c /srv/9de /mnt/9de >[2]/dev/null");

	apply_status_snapshot(p);

	fd = open("/mnt/9de/events", OREAD);
	if(fd < 0)
		return;

	b = Bfdopen(fd, OREAD);
	if(b == nil){
		close(fd);
		return;
	}

	for(;;){
		ln = Brdline(b, '\n');
		if(ln == nil)
			break;
		ln[Blinelen(b)-1] = 0;

		if(strncmp(ln, "ok ", 3) == 0){
			p->de_up = 1;
			snprint(p->de_label, sizeof p->de_label, "de: ok");
			if(strncmp(ln, "ok setpreset ", 13) == 0)
				snprint(p->preset_label, sizeof p->preset_label, "style: %s", ln+13);
			if(strcmp(ln, "ok reload") == 0 || strcmp(ln, "ok apply") == 0)
				p->need_reload = 1;
			markdirty(p);
		}else if(strncmp(ln, "err ", 4) == 0){
			p->de_up = 1;
			snprint(p->de_label, sizeof p->de_label, "de: err");
			p->notif_count++;
			markdirty(p);
		}
	}

	Bterm(b);
}

static void
startsrvwatcher(Panel *p)
{
	int pid;

	if(!p->enable_watch)
		return;

	pid = rfork(RFPROC|RFMEM|RFNOWAIT);
	if(pid == 0){
		srvwatch(p);
		exits(nil);
	}
}

/* ----------------- panel text drawing (topbar aware) ----------------- */

static void
panelstring(Panel *p, Point pt, char *s)
{
	Font *f = p->ui.font ? p->ui.font : font;
	Image *shadow = ui9img(&p->ui, Ui9CShadow);
	Image *text = ui9img(&p->ui, Ui9CTopbarText);

	/* shadow alpha can be 0; still safe */
	string(p->ui.dst, addpt(pt, Pt(0,1)), shadow, ZP, f, s);
	string(p->ui.dst, pt, text, ZP, f, s);
}

static void
ministring(Panel *p, Point pt, char *s, int muted)
{
	Font *f = p->ui.font ? p->ui.font : font;
	Image *shadow = ui9img(&p->ui, Ui9CShadow);
	Image *text = ui9img(&p->ui, muted ? Ui9CMuted : Ui9CText);
	string(p->ui.dst, addpt(pt, Pt(0,1)), shadow, ZP, f, s);
	string(p->ui.dst, pt, text, ZP, f, s);
}

/* ----------------- modules ----------------- */

static void mod_menu_init(Panel*, Pmod*);
static void mod_ws_init(Panel*, Pmod*);
static void mod_de_init(Panel*, Pmod*);
static void mod_net_init(Panel*, Pmod*);
static void mod_clock_init(Panel*, Pmod*);
static void mod_preset_init(Panel*, Pmod*);
static void mod_notif_init(Panel*, Pmod*);

static int  mod_menu_measure(Panel*, Pmod*);
static int  mod_ws_measure(Panel*, Pmod*);
static int  mod_text_measure(Panel*, Pmod*);

static void mod_menu_draw(Panel*, Pmod*, Image*, Rectangle, int, int);
static void mod_ws_draw(Panel*, Pmod*, Image*, Rectangle, int, int);
static void mod_text_draw(Panel*, Pmod*, Image*, Rectangle, int, int);
static void mod_notif_draw(Panel*, Pmod*, Image*, Rectangle, int, int);

static int  mod_menu_hit(Panel*, Pmod*, Mouse*, Rectangle);
static int  mod_notif_hit(Panel*, Pmod*, Mouse*, Rectangle);

static char*
modlabel(Panel *p, Pmod *m)
{
	if(strcmp(m->name, "ws") == 0)     return p->ws_label;
	if(strcmp(m->name, "de") == 0)     return p->de_label;
	if(strcmp(m->name, "net") == 0)    return p->net_label;
	if(strcmp(m->name, "clock") == 0)  return p->clock_label;
	if(strcmp(m->name, "preset") == 0) return p->preset_label;
	return "";
}

static Pmod allmods[] = {
	{ "menu",  0, 0, 1, mod_menu_init,   mod_menu_measure,  mod_menu_draw,  mod_menu_hit,  },
	{ "ws",    0, 0, 1, mod_ws_init,     mod_ws_measure,    mod_ws_draw,    nil,           },
	{ "preset",1, 0, 0, mod_preset_init, mod_text_measure,  mod_text_draw,  nil,           },
	{ "de",    1, 0, 0, mod_de_init,     mod_text_measure,  mod_text_draw,  nil,           },
	{ "net",   1, 0, 0, mod_net_init,    mod_text_measure,  mod_text_draw,  nil,           },
	{ "clock", 1, 0, 0, mod_clock_init,  mod_text_measure,  mod_text_draw,  nil,           },
	{ "notif", 1, 0, 1, mod_notif_init,  mod_text_measure,  mod_notif_draw, mod_notif_hit, },
};

static void mod_menu_init(Panel *p, Pmod *m){ USED(p); USED(m); }
static void mod_ws_init(Panel *p, Pmod *m){ USED(m); updatewins(p); }
static void mod_de_init(Panel *p, Pmod *m){ USED(m); fmtde(p); }
static void mod_net_init(Panel *p, Pmod *m){ USED(m); fmtnet(p); }
static void mod_clock_init(Panel *p, Pmod *m){ USED(m); fmtclock(p); }
static void mod_preset_init(Panel *p, Pmod *m){ USED(m); fmtpreset(p); }
static void mod_notif_init(Panel *p, Pmod *m){ USED(p); USED(m); }

static int
mod_menu_measure(Panel *p, Pmod *m)
{
	Font *f = p->ui.font ? p->ui.font : font;
	char buf[64];
	snprint(buf, sizeof buf, "%s 9DE", sym(p, "≡", "MENU"));
	m->w = stringwidth(f, buf) + p->pad*2;
	return m->w;
}

static int
mod_ws_measure(Panel *p, Pmod *m)
{
	Font *f = p->ui.font ? p->ui.font : font;
	char buf[MaxStr];
	char fit[MaxStr];

	ellipsize(p, fit, sizeof fit, p->ws_label, p->ws_maxw - 40, f);
	snprint(buf, sizeof buf, "%s %s", sym(p, "▦", "WS"), fit);

	m->w = stringwidth(f, buf) + p->pad*2;
	if(m->w > p->ws_maxw) m->w = p->ws_maxw;
	return m->w;
}

static int
mod_text_measure(Panel *p, Pmod *m)
{
	Font *f = p->ui.font ? p->ui.font : font;
	char tmp[MaxStr];
	char *s = modlabel(p, m);

	if(strcmp(m->name, "notif") == 0){
		if(p->notif_count > 0)
			snprint(tmp, sizeof tmp, "%s %d", sym(p, "•", "!"), p->notif_count);
		else
			snprint(tmp, sizeof tmp, "%s", sym(p, "•", "!"));
		s = tmp;
	}

	m->w = stringwidth(f, s) + p->pad*2;
	return m->w;
}

/* chip drawing: calm idle; only show surface on hover/press unless panel_chip=0 */
static void
draw_chip(Panel *p, Rectangle r, int hover, int pressed, int focus)
{
	int rad = p->ui.theme.radius;
	if(rad > 6) rad = 6;

	if(!p->chip || hover || pressed || focus){
		if(pressed)
			ui9_card2(&p->ui, r, rad);
		else
			ui9_card(&p->ui, r, rad);

		if(hover)
			border(p->ui.dst, r, 1, ui9img(&p->ui, Ui9CAccent), ZP);
		else if(focus)
			border(p->ui.dst, r, 1, ui9img(&p->ui, Ui9CAccent), ZP);
	}
}

static void
mod_menu_draw(Panel *p, Pmod *m, Image *dst, Rectangle r, int hover, int pressed)
{
	char buf[64];
	Point pt;

	USED(dst); USED(m);
	draw_chip(p, r, hover, pressed, p->focus_menu);

	snprint(buf, sizeof buf, "%s 9DE", sym(p, "≡", "MENU"));
	pt = Pt(r.min.x + p->pad, r.min.y + (Dy(r)-((p->ui.font?p->ui.font:font)->height))/2);
	panelstring(p, pt, buf);
}

static void
mod_ws_draw(Panel *p, Pmod *m, Image *dst, Rectangle r, int hover, int pressed)
{
	Font *f = p->ui.font ? p->ui.font : font;
	char buf[MaxStr], fit[MaxStr];
	Point pt;

	USED(dst); USED(m);
	draw_chip(p, r, hover, pressed, 0);

	ellipsize(p, fit, sizeof fit, p->ws_label, Dx(r) - (p->pad*2 + 24), f);
	snprint(buf, sizeof buf, "%s %s", sym(p, "▦", "WS"), fit);

	pt = Pt(r.min.x + p->pad, r.min.y + (Dy(r)-f->height)/2);
	panelstring(p, pt, buf);

	p->wsrect = r;
}

static void
mod_text_draw(Panel *p, Pmod *m, Image *dst, Rectangle r, int hover, int pressed)
{
	Font *f = p->ui.font ? p->ui.font : font;
	Point pt;
	char *s;

	USED(dst); USED(hover); USED(pressed);

	s = modlabel(p, m);
	pt = Pt(r.min.x + p->pad, r.min.y + (Dy(r)-f->height)/2);
	panelstring(p, pt, s);
}

static void
mod_notif_draw(Panel *p, Pmod *m, Image *dst, Rectangle r, int hover, int pressed)
{
	Font *f = p->ui.font ? p->ui.font : font;
	char buf[MaxStr];
	Point pt;

	USED(dst); USED(m);

	if(p->notif_count > 0){
		draw_chip(p, r, hover, pressed, 0);
		snprint(buf, sizeof buf, "%s %d", sym(p, "•", "!"), p->notif_count);
	}else{
		snprint(buf, sizeof buf, "%s", sym(p, "•", "!"));
	}

	pt = Pt(r.min.x + p->pad, r.min.y + (Dy(r)-f->height)/2);
	panelstring(p, pt, buf);
}

static int
mod_menu_hit(Panel *p, Pmod *m, Mouse *ms, Rectangle r)
{
	USED(p); USED(m);
	if((ms->buttons & 1) && ptinrect(ms->xy, r)){
		ensurelogdir();
		spawnrc("window 80,80,720,560 rc -c '9de-dash >$home/lib/9de/log/9de-dash.log >[2]$home/lib/9de/log/9de-dash.err'");
		return 1;
	}
	return 0;
}

static int
mod_notif_hit(Panel *p, Pmod *m, Mouse *ms, Rectangle r)
{
	USED(m);
	if(p->notif_count <= 0)
		return 0;
	if((ms->buttons & 1) && ptinrect(ms->xy, r)){
		ensurelogdir();
		spawnrc("window 120,120,900,700 rc -c 'echo --- 9de errors ---; ls -l $home/lib/9de/log; echo; tail -n +1 $home/lib/9de/log/*.err'");
		p->notif_count = 0;
		return 1;
	}
	return 0;
}

/* ----------------- mini list (hover) ----------------- */

static void
drawminibar(Panel *p, Rectangle r)
{
	Font *f = p->ui.font ? p->ui.font : font;
	int i, x, w;
	Rectangle rr;
	Point pt;
	char buf[MaxStr], fit[MaxStr];
	int hover, pressed;
	int hid = -1;

	/* background */
	if(p->ui_minigrad){
		ulong c0, c1;
		ulong o0, o1;
		c0 = p->ui.theme.surfacergb;
		c1 = rgb_add(c0, -8);

		o0 = 0; o1 = 0;
		if(p->ui_minigrad0[0] && parsehexrgb(p->ui_minigrad0, &o0)) c0 = o0;
		if(p->ui_minigrad1[0] && parsehexrgb(p->ui_minigrad1, &o1)) c1 = o1;

		ensurevgrad(p->minigrad, &p->minigrad_n, &p->minigrad_c0, &p->minigrad_c1, &p->minigrad_h, Dy(r), c0, c1);
		drawvgrad(p->ui.dst, r, p->minigrad, p->minigrad_n);
	}else{
		draw(p->ui.dst, r, ui9img(&p->ui, Ui9CSurface), nil, ZP);
	}
	border(p->ui.dst, r, 1, ui9img(&p->ui, Ui9CBorder), ZP);

	x = r.min.x + p->gap;
	for(i=0; i<p->nwins; i++){
		ellipsize(p, fit, sizeof fit, p->wins[i].label, p->win_maxw - 44, f);
		snprint(buf, sizeof buf, "%d %s", p->wins[i].id, fit);

		w = stringwidth(f, buf) + p->pad*2;
		if(w > p->win_maxw) w = p->win_maxw;

		rr = Rect(x, r.min.y+2, x+w, r.max.y-2);

		hover = ptinrect(p->mousexy, rr);
		pressed = hover && (p->mousebuttons & 1);

		if(p->wins[i].current){
			ui9_card2(&p->ui, rr, (p->ui.theme.radius>6)?6:p->ui.theme.radius);
			border(p->ui.dst, rr, 1, ui9img(&p->ui, Ui9CAccent), ZP);
		}else{
			draw_chip(p, rr, hover, pressed, 0);
		}

		pt = Pt(rr.min.x + p->pad, rr.min.y + (Dy(rr)-f->height)/2);
		ministring(p, pt, buf, 0);

		if(hover)
			hid = p->wins[i].id;

		x += w + p->gap;
		if(x > r.max.x - 220)
			break;
	}

	p->ws_hover_id = hid;

	if(hid >= 0)
		snprint(buf, sizeof buf, "path: /dev/wsys/%d", hid);
	else
		snprint(buf, sizeof buf, "path: /dev/wsys");

	pt = Pt(r.max.x - stringwidth(f, buf) - p->gap,
	        r.min.y + (Dy(r)-f->height)/2);
	ministring(p, pt, buf, 1);
}

static int
hitminibar(Panel *p, Rectangle r, Mouse *m)
{
	Font *f = p->ui.font ? p->ui.font : font;
	int i, x, w;
	Rectangle rr;
	char buf[MaxStr], fit[MaxStr];

	x = r.min.x + p->gap;
	for(i=0; i<p->nwins; i++){
		ellipsize(p, fit, sizeof fit, p->wins[i].label, p->win_maxw - 44, f);
		snprint(buf, sizeof buf, "%d %s", p->wins[i].id, fit);

		w = stringwidth(f, buf) + p->pad*2;
		if(w > p->win_maxw) w = p->win_maxw;

		rr = Rect(x, r.min.y+2, x+w, r.max.y-2);
		if((m->buttons & 1) && ptinrect(m->xy, rr)){
			focuswin(p->wins[i].id);
			return 1;
		}
		x += w + p->gap;
		if(x > r.max.x - 220)
			break;
	}
	return 0;
}

/* ----------------- layout/draw ----------------- */

static int
findmod(char *name, Pmod **out)
{
	int i;
	for(i=0; i<nelem(allmods); i++){
		if(strcmp(allmods[i].name, name) == 0){
			*out = &allmods[i];
			return 1;
		}
	}
	return 0;
}

static int
splitwords(char *s, char **w, int maxw)
{
	int n;
	char *p;

	n = 0;
	if(s == nil)
		return 0;

	for(p=s; *p && n < maxw; ){
		while(*p==' ' || *p=='\t' || *p=='\n') p++;
		if(*p == 0) break;
		w[n++] = p;
		while(*p && *p!=' ' && *p!='\t' && *p!='\n') p++;
		if(*p == 0) break;
		*p++ = 0;
	}
	return n;
}

static int
sumw(Panel *p, Pmod **mods, int n)
{
	int i, w;
	w = 0;
	for(i=0; i<n; i++){
		w += mods[i]->w;
		if(i != n-1)
			w += p->gap;
	}
	return w;
}

static void
drawpanel(Panel *p, Pmod **leftmods, int nleft, Pmod **rightmods, int nright)
{
	Rectangle wr, tr, mr, rr;
	Image *dst;
	int i, x;
	int hover, pressed;

	wr = p->dst->r;
	p->r = wr;

	tr = wr;
	tr.max.y = tr.min.y + p->baseh;

	bufrealloc(p);
	dst = (p->buf != nil) ? p->buf : p->dst;
	ui9setdst(&p->ui, dst);
	p->ui.dst = dst;

	/* TOPBAR background */
	if(p->ui_topgrad){
		ulong c0, c1;
		ulong o0, o1;

		c0 = p->ui.theme.topbgrgb;
		/* default: gentle lift (dark -> slightly lighter) */
		c1 = rgb_add(c0, (p->ui.theme.style == Ui9StyleTerminal) ? 14 :
		                (p->ui.theme.style == Ui9StyleDark) ? 10 : 8);

		o0 = 0; o1 = 0;
		if(p->ui_topgrad0[0] && parsehexrgb(p->ui_topgrad0, &o0)) c0 = o0;
		if(p->ui_topgrad1[0] && parsehexrgb(p->ui_topgrad1, &o1)) c1 = o1;

		ensurevgrad(p->topgrad, &p->topgrad_n, &p->topgrad_c0, &p->topgrad_c1, &p->topgrad_h, Dy(tr), c0, c1);
		drawvgrad(dst, tr, p->topgrad, p->topgrad_n);
	}else{
		draw(dst, tr, ui9img(&p->ui, Ui9CTopbarBg), nil, ZP);
	}
	border(dst, tr, 1, ui9img(&p->ui, Ui9CBorder), ZP);

	/* left stack */
	x = tr.min.x + p->gap;
	for(i=0; i<nleft; i++){
		rr = Rect(x, tr.min.y+2, x + leftmods[i]->w, tr.max.y-2);
		leftmods[i]->last = rr;

		hover = ptinrect(p->mousexy, rr);
		pressed = hover && (p->mousebuttons & 1);
		leftmods[i]->draw(p, leftmods[i], dst, rr, hover, pressed);

		x += leftmods[i]->w + p->gap;
	}

	/* right stack */
	x = tr.max.x - p->gap - sumw(p, rightmods, nright);
	for(i=0; i<nright; i++){
		rr = Rect(x, tr.min.y+2, x + rightmods[i]->w, tr.max.y-2);
		rightmods[i]->last = rr;

		hover = ptinrect(p->mousexy, rr);
		pressed = hover && (p->mousebuttons & 1);
		rightmods[i]->draw(p, rightmods[i], dst, rr, hover, pressed);

		x += rightmods[i]->w + p->gap;
	}

	/* mini list */
	if(p->expanded && Dy(wr) >= (p->baseh + p->minih)){
		mr = wr;
		mr.min.y = tr.max.y;
		mr.max.y = mr.min.y + p->minih;
		drawminibar(p, mr);
	}

	/* blit */
	if(p->buf != nil)
		draw(p->dst, wr, p->buf, nil, ZP);

	flushimage(display, 1);
	p->dirty = 0;
}

static void
maybeexpand(Panel *p)
{
	int want = p->ws_hover;

	if(want && !p->expanded){
		p->expanded = 1;
		p->h = p->baseh + p->minih;
		setpanelheight(p, p->h);
		p->dirty = 1;
	}else if(!want && p->expanded){
		p->expanded = 0;
		p->h = p->baseh;
		setpanelheight(p, p->h);
		p->dirty = 1;
	}
}

static void
onmouse(Panel *p, Mouse *m, Pmod **leftmods, int nleft, Pmod **rightmods, int nright)
{
	int i;
	Rectangle mr, tr;

	p->mousexy = m->xy;
	p->mousebuttons = m->buttons;

	tr = p->r;
	tr.max.y = tr.min.y + p->baseh;

	p->ws_hover = 0;
	if(ptinrect(m->xy, p->wsrect))
		p->ws_hover = 1;

	if(p->expanded){
		mr = p->r;
		mr.min.y = tr.max.y;
		mr.max.y = mr.min.y + p->minih;
		if(ptinrect(m->xy, mr))
			p->ws_hover = 1;

		if(hitminibar(p, mr, m)){
			markdirty(p);
			return;
		}
	}

	for(i=0; i<nleft; i++){
		if(leftmods[i]->hit != nil && leftmods[i]->hit(p, leftmods[i], m, leftmods[i]->last)){
			markdirty(p);
			return;
		}
	}
	for(i=0; i<nright; i++){
		if(rightmods[i]->hit != nil && rightmods[i]->hit(p, rightmods[i], m, rightmods[i]->last)){
			markdirty(p);
			return;
		}
	}

	if((m->buttons & 1) && nleft > 0 && strcmp(leftmods[0]->name, "menu")==0){
		if(ptinrect(m->xy, leftmods[0]->last))
			p->focus_menu = 1;
		else
			p->focus_menu = 0;
	}

	maybeexpand(p);
}

static void
onkey(Panel *p, Rune r)
{
	switch(r){
	case 'd':
		ensurelogdir();
		spawnrc("window 80,80,720,560 rc -c '9de-dash >$home/lib/9de/log/9de-dash.log >[2]$home/lib/9de/log/9de-dash.err'");
		p->dirty = 1;
		break;
	case 'l':
		ensurelogdir();
		spawnrc("window 120,120,900,700 rc -c 'ls -l $home/lib/9de/log; echo; tail -n +1 $home/lib/9de/log/*.err'");
		p->dirty = 1;
		break;
	case ' ':
	case '\n':
		if(p->focus_menu){
			ensurelogdir();
			spawnrc("window 80,80,720,560 rc -c '9de-dash >$home/lib/9de/log/9de-dash.log >[2]$home/lib/9de/log/9de-dash.err'");
			p->dirty = 1;
		}
		break;
	}
}

/* ----------------- timers ----------------- */

static void
tick1hz(void *arg)
{
	Panel *p = arg;

	fmtclock(p);
	fmtnet(p);
	if(access("/srv/9de", AEXIST) != 0){
		p->de_up = 0;
		snprint(p->de_label, sizeof p->de_label, "de: down");
	}

	markdirty(p);
}

static void
tickws(void *arg)
{
	Panel *p = arg;
	updatewins(p);
	maybeexpand(p);
	markdirty(p);
}

/* ----------------- entry ----------------- */

static void
usage(void)
{
	fprint(2, "usage: 9de-panel\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Panel p;
	Event e;
	Ui9Sched sched;
	vlong now;
	long next;
	char *ltmp, *rtmp;
	char *lw[MaxMods], *rw[MaxMods];
	Pmod *leftmods[MaxMods], *rightmods[MaxMods];
	int nleft, nright;
	int i;
	Font *fnt;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	memset(&p, 0, sizeof p);

	setcfgdefaults(&p);

	/* load config file */
	loadconfig(&p);

	/* env overrides (dev / session can export) */
	if(getenv("panel_left") != nil)   strecpy(p.panel_left_cfg, p.panel_left_cfg+sizeof p.panel_left_cfg, getenv("panel_left"));
	if(getenv("panel_right") != nil)  strecpy(p.panel_right_cfg, p.panel_right_cfg+sizeof p.panel_right_cfg, getenv("panel_right"));
	if(getenv("panel_height") != nil) p.baseh = atoi(getenv("panel_height"));
	if(getenv("panel_minih") != nil)  p.minih = atoi(getenv("panel_minih"));
	if(getenv("panel_ascii") != nil)  p.ascii = 1;

	/* allow env for appearance too */
	if(getenv("ui_style") != nil)     strecpy(p.ui_style_name, p.ui_style_name+sizeof p.ui_style_name, getenv("ui_style"));
	if(getenv("ui_alpha") != nil)     p.ui_alpha = atoi(getenv("ui_alpha"));
	if(getenv("ui_accent") != nil)    strecpy(p.ui_accent, p.ui_accent+sizeof p.ui_accent, getenv("ui_accent"));
	if(getenv("ui_topbg") != nil)     strecpy(p.ui_topbg, p.ui_topbg+sizeof p.ui_topbg, getenv("ui_topbg"));
	if(getenv("ui_toptext") != nil)   strecpy(p.ui_toptext, p.ui_toptext+sizeof p.ui_toptext, getenv("ui_toptext"));
	if(getenv("ui_border_alpha") != nil) p.ui_border_alpha = atoi(getenv("ui_border_alpha"));
	if(getenv("ui_shadow_alpha") != nil) p.ui_shadow_alpha = atoi(getenv("ui_shadow_alpha"));
	if(getenv("ui_radius") != nil)    p.ui_radius = atoi(getenv("ui_radius"));
	if(getenv("ui_font") != nil)      strecpy(p.ui_font_path, p.ui_font_path+sizeof p.ui_font_path, getenv("ui_font"));

	if(getenv("ui_topgrad") != nil)   p.ui_topgrad = atoi(getenv("ui_topgrad")) != 0;
	if(getenv("ui_topgrad0") != nil)  strecpy(p.ui_topgrad0, p.ui_topgrad0+sizeof p.ui_topgrad0, getenv("ui_topgrad0"));
	if(getenv("ui_topgrad1") != nil)  strecpy(p.ui_topgrad1, p.ui_topgrad1+sizeof p.ui_topgrad1, getenv("ui_topgrad1"));

	if(getenv("ui_minigrad") != nil)  p.ui_minigrad = atoi(getenv("ui_minigrad")) != 0;
	if(getenv("ui_minigrad0") != nil) strecpy(p.ui_minigrad0, p.ui_minigrad0+sizeof p.ui_minigrad0, getenv("ui_minigrad0"));
	if(getenv("ui_minigrad1") != nil) strecpy(p.ui_minigrad1, p.ui_minigrad1+sizeof p.ui_minigrad1, getenv("ui_minigrad1"));

	if(p.baseh <= 16) p.baseh = PanelHDefault;
	if(p.minih < 16)  p.minih = MiniHDefault;
	p.h = p.baseh;

	if(initdraw(0, 0, "9de-panel") < 0)
		sysfatal("initdraw: %r");

	einit(Emouse|Ekeyboard|Eresize);

	/* optional font override */
	fnt = font; /* draw(3) default */
	if(p.ui_font_path[0] != 0){
		Font *nf = openfont(display, p.ui_font_path);
		if(nf != nil)
			fnt = nf;
		else
			fprint(2, "9de-panel: openfont %s failed: %r\n", p.ui_font_path);
	}

	ui9init(&p.ui, display, fnt);

	p.dst = screen;
	ui9setdst(&p.ui, p.dst);

	applyappearance(&p, fnt);

	fmtclock(&p);
	fmtnet(&p);
	fmtde(&p);
	fmtpreset(&p);
	updatewins(&p);

	setpanelheight(&p, p.h);
	startsrvwatcher(&p);

	initmods(&p);

	ui9schedinit(&sched);
	ui9schedadd(&sched, 0, 1000, tick1hz, &p);
	ui9schedadd(&sched, 0, 250, tickws, &p);
	ui9schedadd(&sched, 0, 33, markdirty, &p);

	for(i=0; i<p.nleft; i++) if(p.leftmods[i] && p.leftmods[i]->measure) p.leftmods[i]->measure(&p, p.leftmods[i]);
	for(i=0; i<p.nright; i++) if(p.rightmods[i] && p.rightmods[i]->measure) p.rightmods[i]->measure(&p, p.rightmods[i]);

	p.dirty = 1;

	for(;;){
		now = ui9nowms();
		next = ui9schednext(&sched, now);
		if(next < 0) next = 200;
		alarm(next);

		while(ecanread()){
			switch(event(&e)){
			case Eresize:
				eresized(0);
				p.dst = screen;
				ui9setdst(&p.ui, p.dst);
				bufrealloc(&p);
				p.dirty = 1;
				break;
			case Emouse:
				onmouse(&p, &e.mouse, p.leftmods, p.nleft, p.rightmods, p.nright);
				break;
			case Ekeyboard:
				onkey(&p, e.kbdc);
				break;
			}
		}

		ui9schedtick(&sched, ui9nowms());

		if(p.need_reload){
			p.need_reload = 0;
			reloadpanel(&p, fnt);
		}

		for(i=0; i<p.nleft; i++) if(p.leftmods[i] && p.leftmods[i]->measure) p.leftmods[i]->measure(&p, p.leftmods[i]);
		for(i=0; i<p.nright; i++) if(p.rightmods[i] && p.rightmods[i]->measure) p.rightmods[i]->measure(&p, p.rightmods[i]);

		if(p.dirty)
			drawpanel(&p, p.leftmods, p.nleft, p.rightmods, p.nright);
	}
}
