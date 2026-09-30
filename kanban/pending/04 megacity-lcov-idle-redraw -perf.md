# Stop idle redraw for static MegaCity LCOV coverage

**Source:** `plugins/megacity/product/draxul-megacity/src/megacity_host.cpp`  
**Priority/evidence:** P2; static, high confidence. **Reported by:** Claude. Lines 1886–1894 allow an active LCOV overlay to bypass idle return, then schedule the movement tick; `megacity_plugin.cpp:317–320` requests redraw for the deadline. LCOV import is static between explicit changes, while live perf overlays legitimately refresh.

- [ ] **Baseline:** Count ticks, full draws, and CPU after settling with LCOV on, overlay off, and live perf overlay on.
- [ ] **Implement:** Schedule periodic refresh only for live data, movement, or settling; invalidate once on LCOV import/settings.
- [ ] **Functional safety:** Preserve hover, camera movement, LCOV updates, and live perf display.
- [ ] **Compare:** Require no repeated settled LCOV redraw and report idle CPU before/after.
- [ ] **Platforms:** Check MegaCity on Vulkan and Metal, product aggregate and smoke.
- [ ] **Acceptance:** Static coverage display becomes render-on-change.
