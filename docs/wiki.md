# 9desktop code hierarchy

This page orients you through the source tree so a fresh 9front user can
clone, `mk`, and understand what each piece does.

## Build + run quickstart
- **Build everything:** from repo root, run `mk`. The root `mkfile` descends
  into `lib/` then each `cmd/*` app.
- **Run the UI demo:** `./cmd/ui-demo/ui9demo` (quit with **Q**).
- **Start the early desktop:** `./bin/9de-start.rc` (wraps the built tools).

## Top-level layout
- `mkfile` — orchestrates the library build then each command.
- `bin/` — helper scripts to launch the early desktop stack.
- `include/` — public headers for the UI toolkit.
- `lib/` — UI toolkit implementation (lib9deui.a).
- `cmd/` — apps and utilities built on the toolkit.
- `docs/` — documentation (this page and HTML docs).
- `examples/` — small layout/pattern examples.

## Public headers (`include/9deui/`)
- `9deui.h` — central include for the library; expects callers to include
  `<u.h>`, `<libc.h>`, `<draw.h>` first.
- `theme.h` — color roles, style enums, `Ui9Theme` definition, theming helpers.
- `prim.h` — drawing primitives and helpers.
- `util.h` — generic helpers (strings, math, etc.) used across the library.
- `sched.h` — lightweight scheduler/event utilities.
- `widgets.h` — declarations for all widgets.
- `layout.h` — layout helpers (e.g., minimal flex).
- `icon.h` — icon lookup helpers.
- `frame.h` — focus/capture helpers and ABI stamps.

## UI library implementation (`lib/`)
- `ui.c` — library bootstrap, theme rebuild, and overall UI state handling.
- `theme.c` — preset themes and style switching.
- `prim.c` — rendering primitives.
- `util.c` — utility routines shared across the library.
- `sched.c` — scheduling/timing helpers.
- `frame.c` — focus/capture support.
- `layout.c` — layout utilities.
- `icon.c` — icon table/lookup.
- `widgets/` — individual widget implementations:
  - `button.c`, `toggle.c`, `slider.c`, `segment.c`, `textfield.c`,
    `list.c`, `progress.c`, `dropdown.c`.
- `mkfile` — builds `lib9deui.a` using Plan 9 `mk` + `mklib`.

## Commands (`cmd/`)
- `ui-demo/` — demo app proving the library renders and links.
- `9de-dash/` — dashboard component.
- `9de-panel/` — top bar for the desktop.
- `9de-shell/` — shell integration (9P + threading).
- `9de-session/` — session/config generator.
- `9de-control/` — control center UI.
- Each command has its own `mkfile` (Plan 9 `mkone` based) and builds a single
  binary in-place.

## Examples + docs
- `examples/` — reference layouts and patterns built with the toolkit.
- `docs/index.html` — broader project documentation and milestones.

## Scripts
- `bin/9de-start.rc` — convenience launcher that wires together the built
  components for an early desktop session.

## Build prerequisites (9front)
- Use 9front’s `mk` (not POSIX `make`); the included mkfiles rely on the system
  rules via `</$objtype/mkfile>`.
- Build from a clean clone: `mk` from the repo root should succeed without
  editing files or passing extra flags.
