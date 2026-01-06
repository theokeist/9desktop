#ifndef _9DEUI_FRAME_H_
#define _9DEUI_FRAME_H_

/*
 * frame.h — tiny “app frame” helpers (immediate-mode friendly)
 *
 * This is intentionally small. 9DE apps still own their event loop.
 * The toolkit just provides:
 * - stable focus/capture ids
 * - optional begin/end helpers
 */

enum {
	UI9_ABI = 1,
};

/* stable widget id helpers */
ulong ui9_idstr(char *s);
ulong ui9_idptr(void *p);

/* input snapshots (optional, used by widgets) */
void  ui9_input(Ui9 *ui, Mouse *m, Rune k);

/* focus/capture */
ulong ui9_focus(Ui9 *ui);
void  ui9_setfocus(Ui9 *ui, ulong id);
int   ui9_isfocus(Ui9 *ui, ulong id);

ulong ui9_capture(Ui9 *ui);
void  ui9_setcapture(Ui9 *ui, ulong id);
int   ui9_iscapture(Ui9 *ui, ulong id);
void  ui9_releasecapture(Ui9 *ui);

/* optional frame begin/end */
void  ui9_begin(Ui9 *ui, Image *dst);
void  ui9_end(Ui9 *ui);

#endif
