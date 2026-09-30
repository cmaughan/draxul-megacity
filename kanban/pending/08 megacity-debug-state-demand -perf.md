# Build MegaCity debug state only for consumers

**Source:** `plugins/megacity/product/draxul-megacity/src/megacity_host.cpp`  
**Priority/evidence:** P2; static, medium-high confidence. **Reported by:** Claude, narrowed. Lines 1070–1085 already skip when UI panels are hidden, but visible collapsed panels still cause `metrics_overlay_controller.cpp:156–168` and `live_city_metrics.cpp:318–353` to traverse model/layers and construct keys. Debug data can be useful with overlay `None`.

- [ ] **Baseline:** Count debug builds, allocations, and frame CPU with panels hidden, collapsed, and expanded during motion.
- [ ] **Implement:** Pass actual panel demand or cache by model, mode, runtime, and LCOV revision.
- [ ] **Functional safety:** Keep expanded debug panels live and correct with overlay `None`.
- [ ] **Compare:** Require no unchanged collapsed-panel rebuild and report before/after cost.
- [ ] **Platforms:** Check MegaCity UI on Vulkan and Metal, aggregate and smoke.
- [ ] **Acceptance:** Debug state is produced when a consumer needs it, not merely because panel UI exists.
