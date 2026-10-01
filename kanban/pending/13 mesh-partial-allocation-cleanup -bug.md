# Clean up partially allocated mesh buffers

**Summary:** Release partially created drawing buffers when allocation fails so retries do not retain unused graphics memory.

**Priority:** 13  
**Severity:** MEDIUM  
**Source:** `plugins/megacity/product/draxul-codeviz-renderer/src/codeviz_vk_resources.cpp`

**Evidence and trigger:** B30; vertex allocation survives failed index allocation and is lost by local foliage replacement handles.

- [ ] **Investigate:** Inventory mesh-upload ownership and failure cleanup in callers.
- [ ] **Fix:** Retain scoped ownership until complete success or destroy partial allocations before returning.
- [ ] **Acceptance:** Repeated index-allocation failures leave no retained vertex allocations; successful uploads still transfer ownership correctly.
- [ ] **Validation:** Run the MegaCity-scoped aggregate, relevant Vulkan rendering checks, and same-cache smoke.
