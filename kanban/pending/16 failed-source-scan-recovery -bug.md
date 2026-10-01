# Make failed source scans recoverable
**Summary:** Recover from a failed source scan so a temporary folder error cannot leave the view permanently waiting.

**Priority:** 16  
**Severity:** MEDIUM  
**Source:** `plugins/megacity/product/draxul-megacity/src/semantic_source_controller.cpp`  
**Reported by:** Claude M12; consensus F50.

**Evidence and trigger:** Iterator failure publishes incomplete state at `treesitter.cpp:833`. Controller line 68 rejects it indefinitely, while line 30 prevents restart. Rebuild cannot retry and visible panes continue periodic wakes.

- [ ] **Investigate:** Distinguish active scanning, completed failure, cancellation, and successful publication.
- [ ] **Fix:** Expose terminal failure, report it, and allow safe reset/retry through Rebuild.
- [ ] **Fix:** Stop periodic scan polling once work has ended unsuccessfully.
- [ ] **Acceptance:** A controlled iterator failure leaves useful diagnostics; repairing the folder and rebuilding completes successfully.
- [ ] **Acceptance:** Failed or canceled partial snapshots are not presented as complete source models.
- [ ] **Validation:** Run the MegaCity-scoped aggregate and same-cache smoke; verify retry and settled scheduling.
