\
#ifndef _9DEUI_ICON_H_
#define _9DEUI_ICON_H_

/*
 * icon.h — icon loading + lookup contract.
 *
 * Search order (app name "acme", size 16):
 *   $home/lib/9de/icons/acme/16/icon.bit
 *   $home/lib/9de/icons/acme/icon.bit
 *   /lib/9de/icons/acme/16/icon.bit
 *   /lib/9de/icons/acme/icon.bit
 *
 * Paths can also be absolute, in which case they are loaded directly.
 */

Image* ui9icon_load(Ui9 *ui, char *path);           /* absolute/relative path */
Image* ui9icon_find(Ui9 *ui, char *app, int size);  /* app name */
Image* ui9icon_any(Ui9 *ui, char *app);             /* best-effort: tries common sizes */

#endif
