# Connect MegaCity preferences to durable plugin storage
**Severity:** HIGH  
**Source:** Codex #14; `plugins/megacity/src/megacity_plugin.cpp:143`.

The wrapper supplies no configuration document, so the host always loads defaults and preference-save calls return without writing.

**Investigation**

- [ ] Inventory renderer and camera preference load/save paths for MegaCity and BioView.

**Fix strategy**

- [ ] Wire product-owned durable configuration through wrapper initialization and preference updates.
- [ ] Account for quiescence callback availability if final saves occur during close.

**Acceptance criteria**

- [ ] Renderer and camera preferences survive pane close/reopen and reload in both modes.
- [ ] Run MegaCity aggregate tests, relevant pane checks, and same-cache smoke.
- [ ] Coordinate any final-save dependency with `kanban/pending/05 plugin-quiescence-final-storage-saves -bug.md`.
