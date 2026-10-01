# Share MegaCity frame preparation and uniform layout

**Summary:** Share MegaCity's preparation of camera, lighting, and material data so Windows and Mac drawing code use consistent calculations.

**Priority:** P1 — Vulkan/Metal uniform records and CPU preparation duplicate shader-facing state.  
**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_render_vk.cpp`  
**Proposed by:** Claude 24. **Owner:** one MegaCity renderer agent.  
**Evidence:** Vulkan GLM and Metal SIMD uniform structures/material packing and camera/AO setup are repeated in existing renderer target.

**Boundary verification**
- [ ] Record shader offsets, sizes, Y convention, shadow bias and per-backend differences.
**Implementation and migration**
- [ ] Add private neutral frame contract and pure preparation in existing renderer; migrate material, camera/AO and common shadow metadata in slices. Leave native upload/recording local.
**Unit tests**
- [ ] Deterministic material/frame values and ABI layout assertions through renderer test-internals.
**Cross-platform validation**
- [ ] Run Vulkan and Metal MegaCity goldens, `--megacity` aggregate and same-cache smoke.
**Agent documentation and tooling**
- [ ] Add shared/native split to MegaCity guide.
**Acceptance criteria**
- [ ] Both backends consume one CPU preparation path with intact shader ABI.
