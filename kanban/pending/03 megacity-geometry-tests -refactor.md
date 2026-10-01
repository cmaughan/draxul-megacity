# Isolate MegaCity geometry tests

**Summary:** Run MegaCity's shape-generation tests separately so testing basic geometry does not require building the full host and renderer.

**Priority:** P2 — pure geometry cases link the host and renderer suite.  
**Source:** `plugins/megacity/cmake/Tests.cmake`  
**Proposed by:** Claude 49, narrowed. **Owner:** one MegaCity test agent. **Depends on:** root card 02.  
**Evidence:** geometry tests use `draxul-geometry` but are in `draxul-test-megacity` with host/renderer links.

**Boundary verification**
- [ ] Classify geometry cases and their exact headers without assuming LCOV/config are pure.
**Implementation and migration**
- [ ] Register `draxul-test-megacity-geometry` against geometry only and include it in product scope/aggregate.
**Unit tests**
- [ ] Preserve case inventory and run the new narrow target.
**Cross-platform validation**
- [ ] Check both platform target closures, `--megacity` aggregate and smoke.
**Agent documentation and tooling**
- [ ] Correct MegaCity model/geometry test commands in its guide.
**Acceptance criteria**
- [ ] Geometry tests build without host or renderer dependencies and run in normal MegaCity scope.
