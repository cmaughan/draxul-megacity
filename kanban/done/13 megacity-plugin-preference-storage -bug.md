# Connect MegaCity preferences to durable plugin storage
**Severity:** HIGH  
**Source:** Codex #14; `plugins/megacity/src/megacity_plugin.cpp:143`.

The wrapper supplies no configuration document, so the host always loads defaults and preference-save calls return without writing.

**Investigation**

- [x] Inventory renderer and camera preference load/save paths for MegaCity and BioView.

**Fix strategy**

- [x] Wire product-owned durable configuration through wrapper initialization and preference updates.
- [x] Account for quiescence callback availability if final saves occur during close.

**Acceptance criteria**

- [x] Renderer and camera preferences survive pane close/reopen and reload in both modes.
- [x] Run MegaCity aggregate tests, relevant pane checks, and same-cache smoke.
- [x] Coordinate any final-save dependency with `kanban/done/05 plugin-quiescence-final-storage-saves -bug.md`.

**Implementation and evidence (2026-09-25)**

- The wrapper now loads a checked, product-owned config document from the plugin config directory, using separate `megacity-preferences.toml` and `bioview-preferences.toml` files. The host saves renderer and camera state back to that same file, merging the latest on-disk document to preserve unrelated edits.
- Close and native reload call `shutdown`, which performs the final save before instance destruction. Card 05's callback-quiescence fix already covers storage callbacks during this phase; this implementation writes through the product-owned config document path.
- The real-module ABI integration passed on Windows Debug: one test, 28 assertions. In both modes it checks a saved valid camera state, edits the persisted renderer setting, reopens the pane, performs a native reload, and verifies the edited setting survives. The all-products Debug aggregate passed 49/49 CTest entries; the MegaCity city-mode render reference passed with zero changed pixels. An isolated BioView fixture rendered through the real plugin in export mode. Same-cache Debug startup passed with `py do.py run debug --console -- --smoke-test`; the fixed 30-second `smoke --skip-build` wrapper timed out on the existing nine-pane Session. Release build and startup also passed.
