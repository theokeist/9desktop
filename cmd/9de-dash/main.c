#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include "../../include/9deui/9deui.h"

/*
 * 9de-dash (early)
 * Dashboard surface opened from 9de-panel.
 *
 * Desktop-only: wide layout, tiles, command box.
 * No mobile patterns. No theming engine: obey presets (terminal/dark/glass).
 */

static Ui9 ui;

static char*
userhome(void)
{
	char *home = getenv("home");
	if(home == nil)
		home = getenv("HOME");
	if(home == nil)
		home = "/usr";
	return home;
}

static char*
runpath(char *name)
{
	static char pathbuf[512];
	snprint(pathbuf, sizeof pathbuf, "%s/lib/9de/run/%s", userhome(), name);
	return pathbuf;
}

static void
ensure_run_dir(void)
{
	char runpath[512];
	snprint(runpath, sizeof runpath, "%s/lib/9de/run", userhome());
	create(runpath, OREAD, DMDIR|0755);
}

static void
writepid(void)
{
	int fd;
	char buf[32];

	ensure_run_dir();
	fd = create(runpath("9de-dash.pid"), OWRITE, 0644);
	if(fd < 0)
		return;
	snprint(buf, sizeof buf, "%d\n", getpid());
	write(fd, buf, strlen(buf));
	close(fd);
}

static void
clearpid(void)
{
	remove(runpath("9de-dash.pid"));
}

static int
onnote(void*, char *msg)
{
	USED(msg);
	clearpid();
	exits(nil);
	return 1;
}

static void
tile(Rectangle r, char *title, char *sub, Image *accent)
{
	draw(screen, r, ui9img(&ui, Ui9CSurface2), nil, ZP);
	border(screen, r, 1, ui9img(&ui, Ui9CBorder), ZP);

	if(accent != nil){
		Rectangle a = r;
		a.max.x = a.min.x + 4;
		draw(screen, a, accent, nil, ZP);
	}

	ui9_shadowstring(&ui, Pt(r.min.x + 10, r.min.y + 10), title);
	ui9_mutedstring(&ui, Pt(r.min.x + 10, r.min.y + 28), sub);
}

static void
redraw(void)
{
	Rectangle r, hdr, cmd, body, c1, c2, c3, big, sc;
	int pad, hdrh, colgap;
	int col1w, col2w, col3w;
	int y, th, ty;

	r = screen->r;
	pad = 14;
	hdrh = 54;
	colgap = 14;

	draw(screen, r, ui9img(&ui, Ui9CBackground), nil, ZP);

	hdr = r;
	hdr.max.y = hdr.min.y + hdrh;
	draw(screen, hdr, ui9img(&ui, Ui9CSurface), nil, ZP);
	border(screen, hdr, 1, ui9img(&ui, Ui9CBorder), ZP);

	ui9_boldstring(&ui, Pt(hdr.min.x + 14, hdr.min.y + 18), "Dashboard");
	ui9_mutedstring(&ui, Pt(hdr.min.x + 150, hdr.min.y + 20), "desktop shell · dash surface");

	cmd = Rect(hdr.max.x - 640, hdr.min.y + 14, hdr.max.x - 14, hdr.min.y + 40);
	draw(screen, cmd, ui9img(&ui, Ui9CSurface), nil, ZP);
	border(screen, cmd, 1, ui9img(&ui, Ui9CBorder), ZP);
	ui9_monosmall(&ui, Pt(cmd.min.x + 10, cmd.min.y + 7), "> open control · logs · style terminal");

	body = r;
	body.min.x += pad;
	body.max.x -= pad;
	body.min.y = hdr.max.y + pad;
	body.max.y -= pad;

	col1w = 360;
	col2w = 560;
	col3w = Dx(body) - col1w - col2w - 2*colgap;
	if(col3w < 200) col3w = 200;

	c1 = body;
	c1.max.x = c1.min.x + col1w;

	c2 = body;
	c2.min.x = c1.max.x + colgap;
	c2.max.x = c2.min.x + col2w;

	c3 = body;
	c3.min.x = c2.max.x + colgap;

	ui9_monosmall(&ui, Pt(c1.min.x, c1.min.y - 2), "LAUNCH");
	y = c1.min.y + 12;
	th = 60;
	tile(Rect(c1.min.x, y, c1.max.x, y+th), "Control Center", "session · placement · styles", ui9img(&ui, Ui9CAccent));
	y += th + 10;
	tile(Rect(c1.min.x, y, c1.max.x, y+th), "Launcher", "search apps · recent", nil);
	y += th + 10;
	tile(Rect(c1.min.x, y, c1.max.x, y+th), "Terminal", "rc + plumber", nil);
	y += th + 10;
	tile(Rect(c1.min.x, y, c1.max.x, y+th), "Files", "browse /home /usr", nil);

	ui9_monosmall(&ui, Pt(c2.min.x, c2.min.y - 2), "SYSTEM");
	big = Rect(c2.min.x, c2.min.y + 12, c2.max.x, c2.min.y + 12 + 150);
	draw(screen, big, ui9img(&ui, Ui9CSurface2), nil, ZP);
	border(screen, big, 1, ui9img(&ui, Ui9CBorder), ZP);
	ui9_boldstring(&ui, Pt(big.min.x + 12, big.min.y + 12), "Session: 9DE (rio)");
	ui9_mutedstring(&ui, Pt(big.min.x + 12, big.min.y + 34), "style preset: terminal · logs: split · autostart: shell");
	ui9_monosmall(&ui, Pt(big.min.x + 12, big.min.y + 66), "cpu 37%   mem 58%   net 21%");

	ty = big.max.y + 12;
	tile(Rect(c2.min.x, ty, c2.min.x + (Dx(c2)/2) - 7, ty+70), "Panel placement", "top/bottom/left", nil);
	tile(Rect(c2.min.x + (Dx(c2)/2) + 7, ty, c2.max.x, ty+70), "Style preset", "terminal/dark/glass", nil);

	ty += 80;
	tile(Rect(c2.min.x, ty, c2.max.x, ty+70), "Logs", "open .err / .log streams", ui9img(&ui, Ui9CGood));

	ui9_monosmall(&ui, Pt(c3.min.x, c3.min.y - 2), "SHELL CONTRACT");
	sc = Rect(c3.min.x, c3.min.y + 12, c3.max.x, c3.min.y + 12 + 220);
	draw(screen, sc, ui9img(&ui, Ui9CSurface2), nil, ZP);
	border(screen, sc, 1, ui9img(&ui, Ui9CBorder), ZP);
	ui9_boldstring(&ui, Pt(sc.min.x + 12, sc.min.y + 12), "Fixed placement, flexible apps");
	ui9_mutedstring(&ui, Pt(sc.min.x + 12, sc.min.y + 34), "Panel/Dash/Notify pinned; apps use stacks/grids.");
	ui9_monosmall(&ui, Pt(sc.min.x + 12, sc.min.y + 66),
		"shell: placement + modules\napps: measure → place\nstyles: presets only");

	flushimage(display, 1);
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND

	if(initdraw(0, 0, "9de-dash") < 0)
		sysfatal("initdraw: %r");

	ui9init(&ui, display, font);
	ui9setdst(&ui, screen);

	atnotify(onnote, 1);
	writepid();

	einit(Emouse|Ekeyboard);

	for(;;){
		redraw();
		Event e;
		event(&e);
	}
}
