# Services UI (Dash / Settings)

Model:
- name
- status (ok/degraded/down)
- last error
- log endpoint
- actions: logs / restart / stop / start

Patterns:
- Virtualized list rows.
- Row layout: [badge] Name · short status (right: actions)
- “Logs” opens an inset surface (tail output).
- Actions map to rc commands or /srv/9de ctl files.
