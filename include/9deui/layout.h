\
#ifndef _9DEUI_LAYOUT_H_
#define _9DEUI_LAYOUT_H_

/*
 * layout.h — minimal desktop layout primitives.
 *
 * Goal: stop hand-placing every Rectangle while keeping the “Plan 9 simple”
 * vibe. This is a small flex-like allocator: fixed + intrinsic + grow.
 *
 * Typical usage:
 *   Ui9Flex fx;
 *   ui9flex_init(&fx, r, Ui9FlexRow, 8, 12);
 *   i0 = ui9flex_fixed(&fx, 160);
 *   i1 = ui9flex_grow(&fx, 1, 220);
 *   i2 = ui9flex_fixed(&fx, 120);
 *   ui9flex_layout(&fx);
 *   drawthing(ui9flex_rect(&fx, i1));
 *   ui9flex_free(&fx);
 */

typedef struct Ui9FlexItem Ui9FlexItem;
typedef struct Ui9Flex Ui9Flex;

enum {
	Ui9FlexRow = 0,
	Ui9FlexCol = 1,
};

enum {
	Ui9FlexFixed = 0,
	Ui9FlexIntrinsic = 1,
	Ui9FlexGrow = 2,
};

struct Ui9FlexItem {
	int kind;   /* Ui9Flex* */
	int v;      /* px for fixed/intrinsic; weight for grow */
	int min;    /* minimum px for grow */
	Rectangle r;
};

struct Ui9Flex {
	Rectangle bounds;
	int dir;
	int gap;
	int pad;

	Ui9FlexItem *it;
	int n;
	int cap;
};

void ui9flex_init(Ui9Flex *fx, Rectangle bounds, int dir, int gap, int pad);
void ui9flex_free(Ui9Flex *fx);

int  ui9flex_fixed(Ui9Flex *fx, int px);
int  ui9flex_intrinsic(Ui9Flex *fx, int px);
int  ui9flex_grow(Ui9Flex *fx, int weight, int minpx);

void ui9flex_layout(Ui9Flex *fx);
Rectangle ui9flex_rect(Ui9Flex *fx, int idx);

#endif
