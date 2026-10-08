# Keep route results attached to the current city
**Summary:** Keep routes tied to their city so rebuilding cannot restore an older grid.

**Priority:** P1  
**Source:** `plugins/megacity/product/draxul-megacity/src/megacity_host.cpp`  
**Reported by:** Claude H4; consensus F25.

**Evidence and trigger:** A rebuild preserves the old grid while starting a new one. Route scheduling at line 1549 can use that old grid with new layout state, and line 744 adopts its result unconditionally.

**Related:** `plugins/megacity/kanban/pending/01 megacity-worker-ownership -refactor.md`.

- [ ] **Investigate:** Trace layout/grid generations, selected-building state, route inputs, and both completion orders.
- [ ] **Fix:** Bind route requests/results to current grid identity and generation; reject replaced-grid results.
- [ ] **Fix:** Request routes only after the matching grid is ready and rearm selection routing when it changes.
- [ ] **Acceptance:** Rebuilding a selected city cannot restore an older grid or leave routes missing because an old request was marked complete.
- [ ] **Validation:** Run the MegaCity-scoped aggregate, relevant route/view checks, and same-cache smoke.
