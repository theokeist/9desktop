# 9DE (early) + lib9deui

This repository is the starting point for a rio-based desktop shell on 9front.

Right now it ships:
- **lib9deui**: a small UI library (glass cards, primitives, basic widgets)
- **ui9demo**: a demo app proving the library links + draws

Next milestones (planned):
- lib9descene (in-window layer stack)
- lib9dewm (rio window manipulation helpers)
- 9bar / 9launch / 9notify + a 9de start script

## Build

From the repo root:

```rc
mk
```

## Run the demo

```rc
./cmd/ui-demo/ui9demo
```

Quit with **Q**.

## Start 9DE (early)

```rc
./bin/9de-start.rc
```

## Using lib9deui in your own program

### Include + link
- Add include path: `-I/path/to/9de/include`
- Link: `/path/to/9de/lib/lib9deui.a -ldraw -levent`

Example include:

```c
#include "9deui/9deui.h"
```

### Minimal setup

```c
Ui9 ui;
ui9init(&ui, display, font);
ui9setdst(&ui, screen);

draw(screen, screen->r, ui9img(&ui, Ui9CBg), nil, ZP);
ui9_card(&ui, Rect(20,20,320,120), 18);
ui9_shadowstring(&ui, Pt(40,50), "Hello 9DE");
flushimage(display, 1);
```

## Install (optional, later)

When the API stabilizes you can install headers into `/sys/include/9deui/`
and the library into `/sys/lib/`. For now, keep it as a project-local SDK.


## Components (early)
- `9de-session`: session/config generator (`cmd/9de-session`)
- `9de-panel`: top bar (`cmd/9de-panel`)

### Repo-local start
```rc
chmod +x ./bin/9de-start.rc
./bin/9de-start.rc
```

## Docs
Open `docs/index.html` in a browser.

## Examples
See `examples/` for patterns and layouts.


## Control Center

Build + run:

```rc
cd $home/src/9de
mk
9de control
```


## Toolkit additions

- Ui9Flex (minimal layout): include/9deui/layout.h
- Icons lookup: include/9deui/icon.h
- Focus/capture helpers + ABI stamp: include/9deui/frame.h
