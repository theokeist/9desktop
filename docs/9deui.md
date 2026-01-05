# lib9deui public API (quick reference)

## Init
- `ui9init(Ui9*, Display*, Font*)`
- `ui9setdst(Ui9*, Image*)`
- `ui9setalpha(Ui9*, int alpha)`

## Theme images (read-only)
- `ui9img(ui, Ui9CBg | Ui9CGlass | ...)`

## Primitives
- `ui9_roundrect(ui, r, rad, fill)`
- `ui9_card(ui, r, rad)`
- `ui9_card2(ui, r, rad)`
- `ui9_shadowstring(ui, Pt(x,y), "text")`

## Widgets
- `ui9_button_draw(...)`
- `ui9_toggle_draw(...)`
- `ui9_slider_draw(...)` + `ui9_slider_value(...)`
- `ui9_segment3_draw(...)` + `ui9_segment3_hit(...)`
- `ui9_textfield_draw(...)`
- `ui9_listitem_draw(...)`
- `ui9_progress_draw(...)`
- `ui9_dropdown_btn_draw(...)`
- `ui9_dropdown_item_draw(...)`
