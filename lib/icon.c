#include <u.h>
#include <libc.h>
#include <draw.h>
#include "../include/9deui/9deui.h"

static Image*
loadimg(Ui9 *ui, char *path)
{
	int fd;
	Image *img;

	if(ui == nil || ui->d == nil || path == nil)
		return nil;

	fd = open(path, OREAD);
	if(fd < 0)
		return nil;
	img = readimage(ui->d, fd, 0);
	close(fd);
	return img;
}

Image*
ui9icon_load(Ui9 *ui, char *path)
{
	return loadimg(ui, path);
}

static char*
home(void)
{
	char *h;
	h = getenv("home");
	if(h == nil) h = getenv("HOME");
	return h;
}

static Image*
tryfind(Ui9 *ui, char *base, char *app, int size)
{
	char p[512];

	if(size > 0){
		snprint(p, sizeof p, "%s/%s/%d/icon.bit", base, app, size);
		if(access(p, AREAD) == 0)
			return loadimg(ui, p);
	}
	snprint(p, sizeof p, "%s/%s/icon.bit", base, app);
	if(access(p, AREAD) == 0)
		return loadimg(ui, p);

	return nil;
}

Image*
ui9icon_find(Ui9 *ui, char *app, int size)
{
	char *h;
	Image *img;

	if(app == nil)
		return nil;

	/* absolute path? */
	if(app[0] == '/' || (app[0]=='.' && (app[1]=='/' || (app[1]=='.' && app[2]=='/'))))
		return loadimg(ui, app);

	h = home();
	if(h != nil){
		char base[256];
		snprint(base, sizeof base, "%s/lib/9de/icons", h);
		img = tryfind(ui, base, app, size);
		if(img != nil)
			return img;
	}

	img = tryfind(ui, "/lib/9de/icons", app, size);
	return img;
}

Image*
ui9icon_any(Ui9 *ui, char *app)
{
	static int sizes[] = { 16, 20, 24, 32, 48, 64, 0 };
	int i;
	Image *img;

	for(i=0; sizes[i]; i++){
		img = ui9icon_find(ui, app, sizes[i]);
		if(img != nil)
			return img;
	}
	return ui9icon_find(ui, app, 0);
}
