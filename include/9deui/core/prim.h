#ifndef _9DEUI_PRIM_H_
#define _9DEUI_PRIM_H_

/* drawing primitives */
void ui9_roundrect(Ui9 *ui, Rectangle r, int rad, Image *fill);
void ui9_card(Ui9 *ui, Rectangle r, int rad);      /* glass fill + border */
void ui9_card2(Ui9 *ui, Rectangle r, int rad);     /* secondary fill + border */

void ui9_shadowstring(Ui9 *ui, Point p, char *s);
void ui9_shadowstring_center(Ui9 *ui, Rectangle r, char *s);

#endif
