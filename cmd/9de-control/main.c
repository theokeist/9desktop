#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

#include <9deui/9deui.h>

/*
 * 9de-control — OS-first Control Center (v0)
 *
 * Brutal goals:
 *   - edit $home/lib/9de/config.rc (name=value only)
 *   - run: 9de-session gen
 *   - poke shell: 9de ctl reload  (panel applies live)
 *
 * No themes marketplace. Presets + a few knobs. Like macOS.
 */

enum {
	MaxLine   = 512,
	MaxField  = 256,
	MaxColor  = 32,
};

typedef struct Cfg Cfg;
struct Cfg {
	char key[64];
	char val[MaxLine];
};

static Ui9 ui;
static Font *fnt;

/* form state */
static int start_shell = 1;
static int start_demo  = 0;

static int session_sel = 0;     /* 0 normal, 1 test, 2 dev */
static int layout_sel  = 0;     /* 0 laptop, 1 ultrawide */

static int style_sel   = 0;     /* 0 terminal, 1 dark, 2 glass */
static int alpha_v     = 220;   /* 0..255 */
static int radius_v    = 10;    /* 0..24 */

static int topgrad_on  = 1;
static int minigrad_on = 1;

static Rune r_panel_left[MaxField];
static int  n_panel_left;
static Rune r_panel_right[MaxField];
static int  n_panel_right;

static Rune r_top0[MaxColor];
static int  n_top0;
static Rune r_top1[MaxColor];
static int  n_top1;
static Rune r_mini0[MaxColor];
static int  n_mini0;
static Rune r_mini1[MaxColor];
static int  n_mini1;

static int focused = 0; /* 0 none, 1 left, 2 right, 3 top0,4 top1,5 mini0,6 mini1 */

static char statusline[256] = "Ready.";

/* -------------- small helpers -------------- */

static char*
userhome(void)
{
	char *home = getenv("home");
	return home ? home : "/usr/glenda";
}

static void
runeset(Rune *dst, int ndst, int *pn, char *s)
{
	Rune r;
	int n = 0;
	while(*s && n < ndst-1){
		s += chartorune(&r, s);
		dst[n++] = r;
	}
	dst[n] = 0;
	*pn = n;
}

static int
rune_add(Rune *dst, int ndst, int *pn, Rune r)
{
	if(*pn >= ndst-1)
		return 0;
	dst[*pn] = r;
	(*pn)++;
	dst[*pn] = 0;
	return 1;
}

static void
rune_bs(Rune *dst, int *pn)
{
	if(*pn <= 0)
		return;
	(*pn)--;
	dst[*pn] = 0;
}

static void
trim(char *s)
{
	char *e;
	while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
	e = s + strlen(s);
	while(e>s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) e--;
	*e = 0;
}

/* strips matching quotes "..." or '...' */
static void
stripquotes(char *s)
{
	int n = strlen(s);
	if(n >= 2){
		if((s[0]=='"' && s[n-1]=='"') || (s[0]=='\'' && s[n-1]=='\'')){
			memmove(s, s+1, n-2);
			s[n-2] = 0;
		}
	}
}

/* normalize list like panel does: trim, stripquotes, allow "(a b)" */
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

/* -------------- config I/O -------------- */

static int
readfile(char *path, char **out)
{
	int fd, n, cap;
	char *buf;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;

	cap = 8192;
	buf = malloc(cap);
	if(buf == nil){
		close(fd);
		return -1;
	}
	n = 0;
	for(;;){
		if(n + 2048 >= cap){
			cap *= 2;
			buf = realloc(buf, cap);
			if(buf == nil){
				close(fd);
				return -1;
			}
		}
		int r = read(fd, buf+n, 2048);
		if(r <= 0)
			break;
		n += r;
	}
	close(fd);
	buf[n] = 0;
	*out = buf;
	return n;
}

static int
writefile_atomic(char *path, char *data)
{
	char tmp[256], bak[256];
	int fd, n, w;

	snprint(tmp, sizeof tmp, "%s.new", path);
	snprint(bak, sizeof bak, "%s.bak", path);

	fd = create(tmp, OWRITE, 0644);
	if(fd < 0)
		return -1;

	n = strlen(data);
	w = write(fd, data, n);
	close(fd);
	if(w != n){
		remove(tmp);
		return -1;
	}

	/* keep a backup (best effort) */
	remove(bak);
	rename(path, bak);
	rename(tmp, path);
	return 0;
}

/* update or append key=value line; keeps other lines unchanged */
static void
cfg_set(char *in, char *key, char *val, char *out, int nout)
{
	char *p, *ln, *eol;
	int found = 0;

	out[0] = 0;
	p = in;

	while(*p){
		ln = p;
		eol = strchr(p, '\n');
		if(eol == nil){
			eol = p + strlen(p);
			p = eol;
		}else{
			p = eol + 1;
		}

		char line[MaxLine];
		int len = eol - ln;
		if(len >= MaxLine) len = MaxLine-1;
		memmove(line, ln, len);
		line[len] = 0;

		/* preserve full original line if not matching key */
		{
			char work[MaxLine];
			snprint(work, sizeof work, "%s", line);

			/* strip comments */
			char *c = strchr(work, '#');
			if(c) *c = 0;
			trim(work);

			if(work[0]){
				char *eq = strchr(work, '=');
				if(eq){
					*eq = 0;
					trim(work);
					if(strcmp(work, key) == 0){
						/* replace */
						found = 1;
						char repl[MaxLine];
						snprint(repl, sizeof repl, "%s=%s\n", key, val);
						strecpy(out + strlen(out), out + nout, repl);
						continue;
					}
				}
			}
		}

		/* keep original line */
		strecpy(out + strlen(out), out + nout, line);
		strecpy(out + strlen(out), out + nout, "\n");
	}

	if(!found){
		char add[MaxLine];
		snprint(add, sizeof add, "%s=%s\n", key, val);
		strecpy(out + strlen(out), out + nout, add);
	}
}

/* convenience: rewrite config applying several cfg_set passes */
static int
cfg_apply(char *path)
{
	char *in;
	char tmpbuf[32768];
	char buf2[32768];
	char s_alpha[16], s_rad[16];
	char s_shell[8], s_demo[8];
	char *modes[3] = { "normal", "test", "dev" };
	char *layouts[2] = { "laptop", "ultrawide" };
	char *styles[3] = { "terminal", "dark", "glass" };

	char panel_left_utf[MaxField*4];
	char panel_right_utf[MaxField*4];
	char top0_utf[MaxColor*4], top1_utf[MaxColor*4], mini0_utf[MaxColor*4], mini1_utf[MaxColor*4];

	in = nil;
	if(readfile(path, &in) < 0){
		/* create a minimal file if missing */
		in = strdup("# 9DE config (name=value only)\n");
		if(in == nil) return -1;
	}

	ui9_runestoutf(panel_left_utf, sizeof panel_left_utf, r_panel_left, n_panel_left);
	ui9_runestoutf(panel_right_utf, sizeof panel_right_utf, r_panel_right, n_panel_right);
	ui9_runestoutf(top0_utf, sizeof top0_utf, r_top0, n_top0);
	ui9_runestoutf(top1_utf, sizeof top1_utf, r_top1, n_top1);
	ui9_runestoutf(mini0_utf, sizeof mini0_utf, r_mini0, n_mini0);
	ui9_runestoutf(mini1_utf, sizeof mini1_utf, r_mini1, n_mini1);

	normalize_list(panel_left_utf);
	normalize_list(panel_right_utf);
	trim(top0_utf); trim(top1_utf); trim(mini0_utf); trim(mini1_utf);

	/* format values (quote list strings) */
	char v_left[MaxLine], v_right[MaxLine];
	snprint(v_left, sizeof v_left, "\"%s\"", panel_left_utf);
	snprint(v_right, sizeof v_right, "\"%s\"", panel_right_utf);

	snprint(s_shell, sizeof s_shell, "%d", start_shell ? 1 : 0);
	snprint(s_demo, sizeof s_demo, "%d", start_demo  ? 1 : 0);
	snprint(s_alpha, sizeof s_alpha, "%d", alpha_v);
	snprint(s_rad, sizeof s_rad, "%d", radius_v);

	cfg_set(in, "start_shell", s_shell, tmpbuf, sizeof tmpbuf);
	cfg_set(tmpbuf, "start_demo", s_demo, buf2, sizeof buf2);

	cfg_set(buf2, "session_mode", modes[session_sel], tmpbuf, sizeof tmpbuf);
	cfg_set(tmpbuf, "test_layout", layouts[layout_sel], buf2, sizeof buf2);

	cfg_set(buf2, "panel_left", v_left, tmpbuf, sizeof tmpbuf);
	cfg_set(tmpbuf, "panel_right", v_right, buf2, sizeof buf2);

	cfg_set(buf2, "ui_style", styles[style_sel], tmpbuf, sizeof tmpbuf);
	cfg_set(tmpbuf, "ui_alpha", s_alpha, buf2, sizeof buf2);
	cfg_set(buf2, "ui_radius", s_rad, tmpbuf, sizeof tmpbuf);

	cfg_set(tmpbuf, "ui_topgrad", topgrad_on ? "1" : "0", buf2, sizeof buf2);
	cfg_set(buf2, "ui_minigrad", minigrad_on ? "1" : "0", tmpbuf, sizeof tmpbuf);

	if(topgrad_on){
		if(top0_utf[0]) cfg_set(tmpbuf, "ui_topgrad0", top0_utf, buf2, sizeof buf2);
		else strecpy(buf2, buf2+sizeof buf2, tmpbuf);
		if(top1_utf[0]) cfg_set(buf2, "ui_topgrad1", top1_utf, tmpbuf, sizeof tmpbuf);
	} else {
		/* keep existing values; just disable */
		strecpy(buf2, buf2+sizeof buf2, tmpbuf);
	}

	if(minigrad_on){
		if(mini0_utf[0]) cfg_set(buf2, "ui_minigrad0", mini0_utf, tmpbuf, sizeof tmpbuf);
		else strecpy(tmpbuf, tmpbuf+sizeof tmpbuf, buf2);
		if(mini1_utf[0]) cfg_set(tmpbuf, "ui_minigrad1", mini1_utf, buf2, sizeof buf2);
	} else {
		strecpy(buf2, buf2+sizeof buf2, tmpbuf);
	}

	if(writefile_atomic(path, buf2) < 0){
		free(in);
		return -1;
	}

	free(in);
	return 0;
}

static void
loadcfg_defaults(void)
{
	/* sane defaults (match panel/session defaults) */
	start_shell = 1;
	start_demo  = 0;
	session_sel = 0;
	layout_sel  = 0;
	style_sel   = 0;
	alpha_v     = 220;
	radius_v    = 10;
	topgrad_on  = 1;
	minigrad_on = 1;

	runeset(r_panel_left, MaxField, &n_panel_left, "menu ws");
	runeset(r_panel_right, MaxField, &n_panel_right, "preset de net clock notif");

	runeset(r_top0, MaxColor, &n_top0, "#0c0c0e");
	runeset(r_top1, MaxColor, &n_top1, "#1a1a20");
	runeset(r_mini0, MaxColor, &n_mini0, "#101014");
	runeset(r_mini1, MaxColor, &n_mini1, "#18181f");
}

static void
loadcfg_fromfile(void)
{
	char path[256];
	char *in, *p;

	snprint(path, sizeof path, "%s/lib/9de/config.rc", userhome());
	in = nil;
	if(readfile(path, &in) < 0)
		return;

	p = in;
	while(*p){
		char *eol = strchr(p, '\n');
		int len;
		if(eol == nil) eol = p + strlen(p);
		len = eol - p;

		char line[MaxLine];
		if(len >= MaxLine) len = MaxLine-1;
		memmove(line, p, len);
		line[len] = 0;

		p = (*eol) ? eol + 1 : eol;

		/* strip comments */
		{
			char *c = strchr(line, '#');
			if(c) *c = 0;
		}
		trim(line);
		if(line[0] == 0)
			continue;

		char *eq = strchr(line, '=');
		if(eq == nil) continue;

		*eq = 0;
		char *key = line;
		char *val = eq+1;
		trim(key);
		trim(val);
		normalize_list(val);

		if(strcmp(key, "start_shell")==0) start_shell = atoi(val)!=0;
		else if(strcmp(key, "start_demo")==0) start_demo = atoi(val)!=0;
		else if(strcmp(key, "session_mode")==0){
			if(strcmp(val,"test")==0) session_sel=1;
			else if(strcmp(val,"dev")==0) session_sel=2;
			else session_sel=0;
		}else if(strcmp(key, "test_layout")==0){
			if(strcmp(val,"ultrawide")==0) layout_sel=1;
			else layout_sel=0;
		}else if(strcmp(key, "panel_left")==0){
			runeset(r_panel_left, MaxField, &n_panel_left, val);
		}else if(strcmp(key, "panel_right")==0){
			runeset(r_panel_right, MaxField, &n_panel_right, val);
		}else if(strcmp(key, "ui_style")==0){
			if(strcmp(val,"dark")==0) style_sel=1;
			else if(strcmp(val,"glass")==0) style_sel=2;
			else style_sel=0;
		}else if(strcmp(key, "ui_alpha")==0){
			alpha_v = atoi(val);
			if(alpha_v<0) alpha_v=0; if(alpha_v>255) alpha_v=255;
		}else if(strcmp(key, "ui_radius")==0){
			radius_v = atoi(val);
			if(radius_v<0) radius_v=0; if(radius_v>24) radius_v=24;
		}else if(strcmp(key, "ui_topgrad")==0) topgrad_on = atoi(val)!=0;
		else if(strcmp(key, "ui_minigrad")==0) minigrad_on = atoi(val)!=0;
		else if(strcmp(key, "ui_topgrad0")==0) runeset(r_top0, MaxColor, &n_top0, val);
		else if(strcmp(key, "ui_topgrad1")==0) runeset(r_top1, MaxColor, &n_top1, val);
		else if(strcmp(key, "ui_minigrad0")==0) runeset(r_mini0, MaxColor, &n_mini0, val);
		else if(strcmp(key, "ui_minigrad1")==0) runeset(r_mini1, MaxColor, &n_mini1, val);
	}

	free(in);
}

/* -------------- UI -------------- */

static Rectangle
row(Rectangle r, int h)
{
	Rectangle o = r;
	o.max.y = o.min.y + h;
	return o;
}

static Rectangle
inset(Rectangle r, int dx)
{
	return insetrect(r, dx);
}

static void
draw(void)
{
	Rectangle r, top, body;
	Rectangle sec1, sec2, sec3;
	Rectangle rr;
	char *modes[3] = { "normal", "test", "dev" };
	char *layouts[3] = { "laptop", "ultrawide", " " };
	char *styles[3] = { "terminal", "dark", "glass" };
	char buf[256];

	draw(screen, screen->r, ui9img(&ui, Ui9CBackground), nil, ZP);

	r = inset(screen->r, 16);

	/* top header */
	top = row(r, 44);
	ui9_roundrect(&ui, top, 10, ui9img(&ui, Ui9CSurface));
	border(screen, top, 1, ui9img(&ui, Ui9CBorder), ZP);
	string(screen, addpt(top.min, Pt(12, 14)), ui9img(&ui, Ui9CTopText), ZP, fnt, "9DE Settings (OS-first)");
	{
		Rectangle b = top;
		b.min.x = b.max.x - 140;
		b = inset(b, 8);
		ui9_button_draw(&ui, b, "Apply", Ui9Primary, Ui9StateNormal);
	}

	body = r;
	body.min.y = top.max.y + 14;

	/* Sections */
	sec1 = row(body, 160);
	sec2 = row(Rect(body.min.x, sec1.max.y+14, body.max.x, body.max.y), 170);
	sec3 = Rect(body.min.x, sec2.max.y+14, body.max.x, body.max.y);

	/* Session */
	ui9_roundrect(&ui, sec1, 10, ui9img(&ui, Ui9CSurface));
	border(screen, sec1, 1, ui9img(&ui, Ui9CBorder), ZP);
	string(screen, addpt(sec1.min, Pt(12, 12)), ui9img(&ui, Ui9CText), ZP, fnt, "Session");

	rr = Rect(sec1.min.x+12, sec1.min.y+40, sec1.max.x-12, sec1.min.y+68);
	ui9_toggle_draw(&ui, rr, "Start shell (9de-shell)", start_shell);

	rr = Rect(sec1.min.x+12, rr.max.y+8, sec1.max.x-12, rr.max.y+36);
	ui9_toggle_draw(&ui, rr, "Debug surfaces (start_demo)", start_demo);

	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec1.min.x+12, rr.max.y+12, sec1.max.x-12, rr.max.y+42);
		char *labs[3] = { modes[0], modes[1], modes[2] };
		ui9_segment3_draw(&ui, seg, labs, session_sel);
		/* note: ui9_segment3_draw writes to seg[]; reuse for hit */
	}
	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec1.min.x+12, sec1.min.y+132, sec1.max.x-12, sec1.min.y+162);
		char *labs[3] = { layouts[0], layouts[1], layouts[2] };
		ui9_segment3_draw(&ui, seg, labs, layout_sel);
		string(screen, addpt(Pt(sec1.min.x+12, sec1.min.y+112), Pt(0,0)), ui9img(&ui, Ui9CMuted), ZP, fnt, "testtools layout");
	}

	/* Panel layout */
	ui9_roundrect(&ui, sec2, 10, ui9img(&ui, Ui9CSurface));
	border(screen, sec2, 1, ui9img(&ui, Ui9CBorder), ZP);
	string(screen, addpt(sec2.min, Pt(12, 12)), ui9img(&ui, Ui9CText), ZP, fnt, "Panel layout");

	{
		Rectangle rl = Rect(sec2.min.x+12, sec2.min.y+40, sec2.max.x-12, sec2.min.y+70);
		ui9_textfield_draw(&ui, rl, r_panel_left, n_panel_left, focused==1, "panel_left (e.g. menu ws)");
		Rectangle rr2 = Rect(sec2.min.x+12, rl.max.y+10, sec2.max.x-12, rl.max.y+40);
		ui9_textfield_draw(&ui, rr2, r_panel_right, n_panel_right, focused==2, "panel_right (e.g. preset de net clock notif)");

		Rectangle pb = Rect(sec2.min.x+12, rr2.max.y+12, sec2.min.x+340, rr2.max.y+42);
		ui9_button_draw(&ui, pb, "Default layout", Ui9Secondary, Ui9StateNormal);

		pb = Rect(pb.max.x+10, pb.min.y, pb.max.x+250, pb.max.y);
		ui9_button_draw(&ui, pb, "Minimal", Ui9Secondary, Ui9StateNormal);

		pb = Rect(pb.max.x+10, pb.min.y, pb.max.x+250, pb.max.y);
		ui9_button_draw(&ui, pb, "Dev layout", Ui9Secondary, Ui9StateNormal);
	}

	/* Appearance */
	ui9_roundrect(&ui, sec3, 10, ui9img(&ui, Ui9CSurface));
	border(screen, sec3, 1, ui9img(&ui, Ui9CBorder), ZP);
	string(screen, addpt(sec3.min, Pt(12, 12)), ui9img(&ui, Ui9CText), ZP, fnt, "Appearance");

	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec3.min.x+12, sec3.min.y+40, sec3.max.x-12, sec3.min.y+70);
		ui9_segment3_draw(&ui, seg, styles, style_sel);
	}

	{
		Rectangle rs = Rect(sec3.min.x+12, sec3.min.y+84, sec3.max.x-12, sec3.min.y+112);
		ui9_slider_draw(&ui, rs, alpha_v);
		snprint(buf, sizeof buf, "alpha %d", alpha_v);
		string(screen, addpt(Pt(rs.min.x, rs.min.y-16), Pt(0,0)), ui9img(&ui, Ui9CMuted), ZP, fnt, buf);
	}

	{
		Rectangle rs = Rect(sec3.min.x+12, sec3.min.y+126, sec3.max.x-12, sec3.min.y+154);
		/* map radius 0..24 into slider 0..255 */
		int sv = (radius_v*255)/24;
		ui9_slider_draw(&ui, rs, sv);
		snprint(buf, sizeof buf, "radius %d", radius_v);
		string(screen, addpt(Pt(rs.min.x, rs.min.y-16), Pt(0,0)), ui9img(&ui, Ui9CMuted), ZP, fnt, buf);
	}

	{
		Rectangle rg = Rect(sec3.min.x+12, sec3.min.y+164, sec3.max.x-12, sec3.min.y+192);
		ui9_toggle_draw(&ui, rg, "Top gradient", topgrad_on);

		Rectangle rt0 = Rect(sec3.min.x+12, rg.max.y+8, (sec3.min.x+sec3.max.x)/2 - 6, rg.max.y+38);
		Rectangle rt1 = Rect((sec3.min.x+sec3.max.x)/2 + 6, rg.max.y+8, sec3.max.x-12, rg.max.y+38);
		ui9_textfield_draw(&ui, rt0, r_top0, n_top0, focused==3, "#rrggbb");
		ui9_textfield_draw(&ui, rt1, r_top1, n_top1, focused==4, "#rrggbb");

		Rectangle rg2 = Rect(sec3.min.x+12, rt0.max.y+10, sec3.max.x-12, rt0.max.y+38);
		ui9_toggle_draw(&ui, rg2, "Mini gradient", minigrad_on);

		Rectangle rm0 = Rect(sec3.min.x+12, rg2.max.y+8, (sec3.min.x+sec3.max.x)/2 - 6, rg2.max.y+38);
		Rectangle rm1 = Rect((sec3.min.x+sec3.max.x)/2 + 6, rg2.max.y+8, sec3.max.x-12, rg2.max.y+38);
		ui9_textfield_draw(&ui, rm0, r_mini0, n_mini0, focused==5, "#rrggbb");
		ui9_textfield_draw(&ui, rm1, r_mini1, n_mini1, focused==6, "#rrggbb");
	}

	/* footer status */
	{
		Rectangle f = Rect(r.min.x, screen->r.max.y-36, r.max.x, screen->r.max.y-12);
		string(screen, f.min, ui9img(&ui, Ui9CMuted), ZP, fnt, statusline);
	}

	flushimage(display, 1);
}

static int
hit_apply(Rectangle top, Point p)
{
	Rectangle b = top;
	b.min.x = b.max.x - 140;
	b = inset(b, 8);
	return ptinrect(p, b);
}

/* hardcoded hit rects must match draw() layout */
static void
onmouse(Mouse m)
{
	Rectangle r = inset(screen->r, 16);
	Rectangle top = row(r, 44);
	Rectangle body = r;
	body.min.y = top.max.y + 14;
	Rectangle sec1 = row(body, 160);
	Rectangle sec2 = row(Rect(body.min.x, sec1.max.y+14, body.max.x, body.max.y), 170);
	Rectangle sec3 = Rect(body.min.x, sec2.max.y+14, body.max.x, body.max.y);

	if((m.buttons & 1) == 0)
		return;

	if(hit_apply(top, m.xy)){
		char path[256];
		snprint(path, sizeof path, "%s/lib/9de/config.rc", userhome());
		if(cfg_apply(path) < 0){
			snprint(statusline, sizeof statusline, "Write failed: %r");
		}else{
			/* regenerate autostart (best effort) */
			system("mkdir -p $home/lib/9de/log; 9de-session gen >$home/lib/9de/log/9de-session.log >[2]$home/lib/9de/log/9de-session.err");
			/* live reload */
			system("9de ctl reload >/dev/null >[2]/dev/null");
			snprint(statusline, sizeof statusline, "Applied. (config.rc updated, session gen, reload sent)");
		}
		draw();
		return;
	}

	/* Session toggles */
	{
		Rectangle rr = Rect(sec1.min.x+12, sec1.min.y+40, sec1.max.x-12, sec1.min.y+68);
		if(ptinrect(m.xy, rr)){
			start_shell = !start_shell;
			draw();
			return;
		}
		rr = Rect(sec1.min.x+12, rr.max.y+8, sec1.max.x-12, rr.max.y+36);
		if(ptinrect(m.xy, rr)){
			start_demo = !start_demo;
			draw();
			return;
		}
	}

	/* mode segmented */
	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec1.min.x+12, sec1.min.y+40+28+8+28+12, sec1.max.x-12, sec1.min.y+40+28+8+28+42);
		char *labs[3] = { "normal", "test", "dev" };
		ui9_segment3_draw(&ui, seg, labs, session_sel);
		int hit = ui9_segment3_hit(seg, m.xy);
		if(hit >= 0){
			session_sel = hit;
			draw();
			return;
		}
	}

	/* test_layout segmented */
	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec1.min.x+12, sec1.min.y+132, sec1.max.x-12, sec1.min.y+162);
		char *labs[3] = { "laptop", "ultrawide", " " };
		ui9_segment3_draw(&ui, seg, labs, layout_sel);
		int hit = ui9_segment3_hit(seg, m.xy);
		if(hit >= 0 && hit <= 1){
			layout_sel = hit;
			draw();
			return;
		}
	}

	/* panel text fields + preset buttons */
	{
		Rectangle rl = Rect(sec2.min.x+12, sec2.min.y+40, sec2.max.x-12, sec2.min.y+70);
		Rectangle rr2 = Rect(sec2.min.x+12, rl.max.y+10, sec2.max.x-12, rl.max.y+40);

		if(ptinrect(m.xy, rl)){ focused = 1; draw(); return; }
		if(ptinrect(m.xy, rr2)){ focused = 2; draw(); return; }

		Rectangle pb = Rect(sec2.min.x+12, rr2.max.y+12, sec2.min.x+340, rr2.max.y+42);
		if(ptinrect(m.xy, pb)){
			runeset(r_panel_left, MaxField, &n_panel_left, "menu ws");
			runeset(r_panel_right, MaxField, &n_panel_right, "preset de net clock notif");
			draw();
			return;
		}
		pb = Rect(pb.max.x+10, pb.min.y, pb.max.x+250, pb.max.y);
		if(ptinrect(m.xy, pb)){
			runeset(r_panel_left, MaxField, &n_panel_left, "menu");
			runeset(r_panel_right, MaxField, &n_panel_right, "clock");
			draw();
			return;
		}
		pb = Rect(pb.max.x+10, pb.min.y, pb.max.x+250, pb.max.y);
		if(ptinrect(m.xy, pb)){
			runeset(r_panel_left, MaxField, &n_panel_left, "menu ws");
			runeset(r_panel_right, MaxField, &n_panel_right, "preset de net clock notif");
			start_demo = 1;
			session_sel = 1;
			draw();
			return;
		}
	}

	/* appearance segmented */
	{
		Rectangle seg[3];
		Rectangle rseg = Rect(sec3.min.x+12, sec3.min.y+40, sec3.max.x-12, sec3.min.y+70);
		char *labs[3] = { "terminal", "dark", "glass" };
		ui9_segment3_draw(&ui, seg, labs, style_sel);
		int hit = ui9_segment3_hit(seg, m.xy);
		if(hit >= 0){
			style_sel = hit;
			draw();
			return;
		}
	}

	/* alpha slider */
	{
		Rectangle rs = Rect(sec3.min.x+12, sec3.min.y+84, sec3.max.x-12, sec3.min.y+112);
		if(ptinrect(m.xy, rs)){
			alpha_v = ui9_slider_value(rs, m.xy);
			draw();
			return;
		}
	}

	/* radius slider (0..24) */
	{
		Rectangle rs = Rect(sec3.min.x+12, sec3.min.y+126, sec3.max.x-12, sec3.min.y+154);
		if(ptinrect(m.xy, rs)){
			int sv = ui9_slider_value(rs, m.xy);
			radius_v = (sv*24)/255;
			draw();
			return;
		}
	}

	/* gradient toggles + fields */
	{
		Rectangle rg = Rect(sec3.min.x+12, sec3.min.y+164, sec3.max.x-12, sec3.min.y+192);
		if(ptinrect(m.xy, rg)){
			topgrad_on = !topgrad_on;
			draw();
			return;
		}

		Rectangle rt0 = Rect(sec3.min.x+12, rg.max.y+8, (sec3.min.x+sec3.max.x)/2 - 6, rg.max.y+38);
		Rectangle rt1 = Rect((sec3.min.x+sec3.max.x)/2 + 6, rg.max.y+8, sec3.max.x-12, rg.max.y+38);
		if(ptinrect(m.xy, rt0)){ focused = 3; draw(); return; }
		if(ptinrect(m.xy, rt1)){ focused = 4; draw(); return; }

		Rectangle rg2 = Rect(sec3.min.x+12, rt0.max.y+10, sec3.max.x-12, rt0.max.y+38);
		if(ptinrect(m.xy, rg2)){
			minigrad_on = !minigrad_on;
			draw();
			return;
		}

		Rectangle rm0 = Rect(sec3.min.x+12, rg2.max.y+8, (sec3.min.x+sec3.max.x)/2 - 6, rg2.max.y+38);
		Rectangle rm1 = Rect((sec3.min.x+sec3.max.x)/2 + 6, rg2.max.y+8, sec3.max.x-12, rg2.max.y+38);
		if(ptinrect(m.xy, rm0)){ focused = 5; draw(); return; }
		if(ptinrect(m.xy, rm1)){ focused = 6; draw(); return; }
	}

	/* click elsewhere clears focus */
	focused = 0;
	draw();
}

static void
onkey(Rune r)
{
	Rune *buf = nil;
	int *pn = nil;
	int nmax = 0;

	if(r == 'q' || r == 'Q')
		exits(nil);

	switch(focused){
	case 1: buf = r_panel_left; pn = &n_panel_left; nmax = MaxField; break;
	case 2: buf = r_panel_right; pn = &n_panel_right; nmax = MaxField; break;
	case 3: buf = r_top0; pn = &n_top0; nmax = MaxColor; break;
	case 4: buf = r_top1; pn = &n_top1; nmax = MaxColor; break;
	case 5: buf = r_mini0; pn = &n_mini0; nmax = MaxColor; break;
	case 6: buf = r_mini1; pn = &n_mini1; nmax = MaxColor; break;
	default:
		return;
	}

	if(r == '\b' || r == 0x7f){
		rune_bs(buf, pn);
		draw();
		return;
	}
	if(r == '\n' || r == '\r')
		return;

	if(r >= 0x20){
		rune_add(buf, nmax, pn, r);
		draw();
	}
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	default:
	}ARGEND

	loadcfg_defaults();
	loadcfg_fromfile();

	if(initdraw(0, 0, "9de-control") < 0)
		sysfatal("initdraw: %r");

	fnt = font;
	ui9init(&ui, display, fnt);
	ui9setdst(&ui, screen);

	einit(Emouse|Ekeyboard|Eresize);

	draw();

	for(;;){
		Event e;
		switch(event(&e)){
		case Emouse:
			onmouse(e.mouse);
			break;
		case Ekeyboard:
			onkey(e.kbdc);
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			ui9setdst(&ui, screen);
			draw();
			break;
		}
	}
}
