#ifndef _9DEUI_WIDGETS_H_
#define _9DEUI_WIDGETS_H_

enum {
	Ui9Primary = 1,
	Ui9Secondary = 2,
};

enum {
	Ui9StateNormal = 0,
	Ui9StatePressed = 1,
	Ui9StateDisabled = 2,
};

/* Buttons */
void ui9_button_draw(Ui9 *ui, Rectangle r, char *label, int kind, int state);

/* Toggle */
void ui9_toggle_draw(Ui9 *ui, Rectangle r, char *label, int on);

/* Slider (0..255) */
void ui9_slider_draw(Ui9 *ui, Rectangle r, int value);
int  ui9_slider_value(Rectangle r, Point p);

/* Segmented control (3 segments for now) */
void ui9_segment3_draw(Ui9 *ui, Rectangle r[3], char *label[3], int sel);
int  ui9_segment3_hit(Rectangle r[3], Point p);

/* Text field */
void ui9_textfield_draw(Ui9 *ui, Rectangle r, Rune *buf, int nbuf, int focused, char *placeholder);

/* List item */
void ui9_listitem_draw(Ui9 *ui, Rectangle r, char *label, int selected);

/* Progress (0..100) */
void ui9_progress_draw(Ui9 *ui, Rectangle r, int pct);

/* Dropdown button + menu items */
void ui9_dropdown_btn_draw(Ui9 *ui, Rectangle r, char *label, int open);
void ui9_dropdown_item_draw(Ui9 *ui, Rectangle r, char *label, int selected);

#endif
