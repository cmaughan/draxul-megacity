# Use defined arithmetic for biological noise

**Summary:** Use safe arithmetic when generating biological shapes so opening the biology view cannot cause invalid calculations.

**Priority:** 11  
**Severity:** CRITICAL  
**Source:** `plugins/megacity/product/draxul-geometry/src/cell_generator.cpp`

**Evidence and trigger:** B01; coordinate products overflow before unsigned conversion under ordinary biology settings.

- [ ] **Investigate:** Trace default noise offsets, octave expansion, and negative-coordinate behavior.
- [ ] **Fix:** Convert coordinates before multiplication and use unsigned constants.
- [ ] **Acceptance:** Default biology construction and boundary coordinates execute without signed overflow and remain deterministic.
- [ ] **Validation:** Run the MegaCity-scoped aggregate, relevant biology rendering checks, and same-cache smoke; use undefined-behavior instrumentation where available.
