# Separate outstanding sign-image uploads

**Summary:** Give pending sign-image uploads separate storage so quick color changes cannot replace pixels still being copied.

**Priority:** P1  
**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_render_vk.cpp`

**Evidence and trigger:** B21; same-sized atlas revisions overwrite shared staging while an earlier frame may still copy it.

- [ ] **Investigate:** Trace revisions, slot completion, and staging retirement across automatic color rebuilds.
- [ ] **Fix:** Use staging per frame slot or separately retired allocations per revision.
- [ ] **Acceptance:** Outstanding same-sized revisions retain their own source bytes until completion and render the intended labels.
- [ ] **Validation:** Preserve Metal behavior; run the MegaCity-scoped aggregate, Vulkan label checks, and same-cache smoke.
