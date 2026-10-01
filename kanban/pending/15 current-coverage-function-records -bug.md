# Parse coverage function records correctly
**Summary:** Read coverage records correctly so executed functions are not silently shown as uncovered.

**Priority:** 15  
**Severity:** MEDIUM  
**Source:** `plugins/megacity/product/draxul-megacity/src/lcov_coverage.cpp`  
**Reported by:** Claude M11; consensus F49.

**Evidence and trigger:** Line 107 includes an optional end-line number in the function name; indexed `FNL`/`FNA` records are ignored. Both forms are documented coverage inputs.

- [ ] **Investigate:** Trace function names/counts from tracefile import into model matching and overlay presentation.
- [ ] **Fix:** Parse optional numeric fields and indexed leaders/aliases while preserving complete function names.
- [ ] **Acceptance:** Documented records preserve function names and execution counts, including names containing commas.
- [ ] **Acceptance:** Unsupported or malformed records produce controlled behavior without silently misrepresenting accepted records.
- [ ] **Validation:** Run the MegaCity-scoped aggregate, coverage overlay checks, and same-cache smoke.
