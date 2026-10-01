# Skip unchanged MegaCity point-shadow cube passes

**Summary:** Reuse MegaCity's point-light shadows when their inputs are unchanged so moving only the camera does not redraw the same shadows.

**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_render_vk.cpp`  
**Priority/evidence:** P2; static, medium confidence. **Reported by:** Claude. Lines 3572–3660 record valid cube faces every frame without a revision gate. Camera-only movement need not change fixed light/caster shadows; pass eligibility varies, so a universal draw-count multiplier is unsupported.

- [ ] **Baseline:** Count cube-pass draws and GPU duration during camera-only movement and scene/light edits.
- [ ] **Implement:** Track per-slot initialized shadow revisions and invalidate on caster, opacity/material, light, or target changes.
- [ ] **Functional safety:** Preserve in-flight slot writes, selection modes, shadow correctness, and recreation.
- [ ] **Compare:** Require unchanged camera-only frames to skip cube work while changed shadows update.
- [ ] **Platforms:** Verify Vulkan and inspect/cover equivalent Metal cube recording, MegaCity aggregate and smoke.
- [ ] **Acceptance:** Point shadows render only when their inputs or target require it.
