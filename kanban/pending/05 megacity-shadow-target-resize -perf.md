# Retain fixed-resolution MegaCity shadows during pane resize

**Summary:** Keep MegaCity's fixed-size shadow images when resizing a pane so dragging a divider does not repeatedly recreate resources whose size has not changed.

**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_render_vk.cpp`  
**Priority:** P2; static, high confidence. **Reported by:** Claude. `ensure_gbuffer_targets()` at lines 2851–2924 waits idle, destroys all targets, then reallocates fixed-resolution shadow cascades on an extent change. Metal `codeviz_render.mm:988–1018` has the same combined lifetime. The shadow maps’ size is independent of pane size.

- [ ] **Baseline:** Count target creations/bytes, device waits, resize p95, and peak VRAM with fixed shadow settings.
- [ ] **Implement:** Separate per-slot shadow lifetime from viewport-sized G-buffer lifetime and retire old extent targets safely.
- [ ] **Functional safety:** Preserve frame-count changes, shadows, resize failure, and in-flight slot ownership; do not share shadows across slots without barrier/overlap analysis.
- [ ] **Compare:** Require no shadow reallocation on extent-only drag.
- [ ] **Platforms:** Check Vulkan and Metal resize/render goldens, MegaCity aggregate and smoke.
- [ ] **Acceptance:** Pane resizing replaces only size-dependent targets.
