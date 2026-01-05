## Toolkit v10 — layout + icons + focus helpers

- Adds minimal flex layout (`include/9deui/layout.h`, `lib/layout.c`)
- Adds icon lookup contract (`include/9deui/icon.h`, `lib/icon.c`)
- Adds frame helpers (focus/capture ids, input snapshot, `UI9_ABI`) (`include/9deui/frame.h`, `lib/frame.c`)
- `Ui9` now carries optional input + focus state (still safe for existing apps)

## OS-first Control Center

- Adds `9de-control` GUI to edit `config.rc` and apply changes.
- `Apply` runs `9de-session gen` and sends `reload` via `/srv/9de`.
- `9de-panel` reloads config + module order on `ok reload`.

## Session modes + test/dev tools

- Adds session_mode=normal|test|dev (handled by 9de-riostart).
- Adds `9de testtools` and `9de devtools`.
- Fixes `9de start` to source defaults/config before autostart.
- Updates autostart generation so start_demo is runtime (no regen needed).

## 9de-panel v6 (gradients + appearance knobs)

- Adds controlled gradients for topbar + mini list bar (config keys ui_topgrad/ui_minigrad + endpoints).
- Fixes/implements config parsing for ui_* appearance keys directly in panel.
- Uses topbar text color role for topbar labels.

## 9de-panel v4 (good UI + tokens)

- Adds configurable UI tokens: panel_pad, panel_gap, panel_chip, panel_ws_maxw, panel_win_maxw.
- Default style is calm idle: chips appear on hover/press.
- Ellipsizes ws and mini list labels.
- Adds consistent spacing between modules.

## 9de-panel v3 (OS-level)

- Adds ws module (current rio window label) from /dev/wsys.
- Hover on ws expands panel and shows horizontal mini window list; click focuses via /dev/wsys/<id>/wctl.
- Shows system path hints in mini bar.

## 9de-panel v2 ($25)

- Modular panel with hover/pressed states.
- Reads $home/lib/9de/config.rc (minimal parser) for panel_* keys.
- Adds preset + notif modules; watcher parses setpreset and counts err events.

