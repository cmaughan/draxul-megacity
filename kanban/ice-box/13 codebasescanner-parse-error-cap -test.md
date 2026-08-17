# Bound CodebaseScanner parse-error reporting

**Type:** test

## Current state

MegaCity's Tree-sitter tests already cover scanner restart and generated/dependency
directory skipping. The remaining gap is explicit coverage of the per-file parse-error
cap (`kMaxErrorsPerFile`) while still delivering a completed snapshot.

## Work

- [ ] Generate a deterministic source file with more parse errors than the cap.
- [ ] Verify the delivered snapshot reports no more than the configured maximum.
- [ ] Verify valid files in the same scan still contribute semantic results.
- [ ] Run the focused MegaCity Tree-sitter tests and sanitizer coverage available in
      the product repository.
