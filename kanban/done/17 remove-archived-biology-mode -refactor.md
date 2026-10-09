# Retire the archived biology visualization

**Summary:** Remove BioView's builders, organic geometry, configuration mode, UI and tests while preserving MegaCity City scanning, semantics, layout, rendering, preferences and controls.

**Priority:** P1 — user-requested removal of an archived experiment

- [x] Preserve the prior implementation in Git and an external archive.
- [x] Remove mode-dependent host/plugin paths, biology builders and exclusive geometry/UI.
- [x] Retain meaningful City lifecycle, preferences, scene and geometry coverage; remove obsolete render fixtures and documentation.
- [x] Run the core + MegaCity aggregate, same-cache startup and available City renders; inspect shared Windows/Metal behavior and record runtime gaps.

Root integration is tracked in `kanban/done/119 remove-archived-experiments -refactor.md` in Draxul. No compatibility aliases or migration are required for the retired mode.

Preserved checkout: `/Users/cmaughan/dev/archived-experiments/draxul-pcbview` (origin remains the existing GitHub repository). MegaCity history bundle: `/Users/cmaughan/dev/archived-experiments/draxul-megacity-before-bio-removal-20261009.bundle`. Original product revision: `06c804c11cdda5de6e2c5d986fdbe33c1e85da1b`.

Implementation: the remaining City host owns one preferences file and UI layout; mode-dependent helpers, analysis controls, builders, exclusive ellipsoid/sphere/organic geometry and tests were removed. The shared semantic scanner, City layout/routing, Vulkan/Metal render passes and City preference/reload integration coverage remain.

CI: root checkout now initializes all five remaining public submodules recursively. MegaCity, SatView and ScoreView standalone CI no longer sets the retired product option and explicitly disables unmounted Flashcards. Windows native execution is unavailable on this Mac; backend sources and shared compile boundaries were inspected.

Acceptance: core + MegaCity aggregate passed 60/60 (27.39s tests, 52.39s build + tests), same-cache smoke passed and final isolated Release startup passed (5.34s build/startup). City rendered before and after removal to byte-identical BMPs. The existing golden fails equally at 82.6172%; its remaining reference-provenance work is transferred to `kanban/pending/18 city-render-reference-mismatch -test.md`. Windows runtime execution is deferred to normal CI, not claimed as a local pass. See the root removal card for complete timings and diagnostics.
