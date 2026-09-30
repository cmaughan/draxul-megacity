# Reuse MegaCity world records on camera-only frames

**Source:** `plugins/megacity/product/draxul-megacity/src/megacity_host.cpp`  
**Priority/evidence:** P2; static, high confidence. **Reported by:** Claude, Codex. Lines 1499–1505 mark the scene dirty on camera movement; 1559–1573 publish a full snapshot. `scene_snapshot_builder.cpp:245–338,464–493,536` revisits entities, material lookups, strings, and sort order during pan/orbit.

- [ ] **Baseline:** Count full ECS snapshot builds and GUI CPU during a fixed camera replay as entity count grows.
- [ ] **Implement:** Retain object/material/mesh records; recompute camera-dependent matrices, bounds, depth/order, and selection opacity.
- [ ] **Functional safety:** Preserve transparent order, labels, hover/selection, and model/material invalidation.
- [ ] **Compare:** Require zero full ECS rebuilds after priming for camera-only frames.
- [ ] **Platforms:** Check MegaCity Vulkan and Metal views, aggregate and smoke.
- [ ] **Acceptance:** Camera motion updates camera-dependent state without reconstructing the world.
