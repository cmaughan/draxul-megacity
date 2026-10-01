# Batch MegaCity Metal material texture preparation

**Summary:** Prepare MegaCity's material images together on Mac so loading them does not stop and wait for the graphics card after each image.

**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_render.mm`  
**Priority/evidence:** P2; static, medium-high confidence. **Reported by:** Claude. Lines 669–683 create, commit, and wait for a mip-generation command buffer per material texture during cold setup. Vulkan records equivalent preparation without a per-texture wait.

- [ ] **Baseline:** Count texture submissions/waits and cold-start frame p95 as material texture count grows.
- [ ] **Implement:** Batch upload and mip work in the owned preparation command stream, retaining inputs until completion.
- [ ] **Functional safety:** Preserve mip sampling, resource lifetime, missing-material fallback, and error reporting.
- [ ] **Compare:** Require no per-material blocking wait and report startup latency before/after.
- [ ] **Platforms:** Check Metal output and lifetime; retain Vulkan behavior and run MegaCity aggregate/smoke.
- [ ] **Acceptance:** Cold material setup avoids serial GPU waits per texture.
