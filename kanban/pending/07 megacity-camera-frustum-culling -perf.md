# Cull offscreen MegaCity objects before camera draws

**Source:** `plugins/megacity/product/draxul-megacity/src/scene_snapshot_builder.cpp`  
**Priority/evidence:** P2; static, high confidence. **Reported by:** Codex. Lines 327–338 retain all renderables; Vulkan `codeviz_render_vk.cpp:3712–3718,3913–3925` and Metal `codeviz_render.mm:1700–1765` record camera-pass draws without frustum rejection. Hardware clipping comes after CPU recording and vertex work.

- [ ] **Baseline:** Hold visible content fixed while increasing offscreen objects; count camera-pass draws and frame CPU/GPU time.
- [ ] **Implement:** Derive conservative object bounds and a camera-visible list; keep shadow-caster visibility independent.
- [ ] **Functional safety:** Check boundary objects, custom meshes, labels, transparency, and camera motion.
- [ ] **Compare:** Require camera draws to follow intersecting objects, reporting before/after frame cost.
- [ ] **Platforms:** Verify Vulkan and Metal render goldens, MegaCity aggregate and smoke.
- [ ] **Acceptance:** Offscreen objects are not recorded for main camera passes.
