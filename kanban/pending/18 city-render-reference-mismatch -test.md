# Reconcile the City render fixture and reference

**Summary:** Establish a reviewed, reproducible City render reference so the existing macOS golden comparison provides useful regression coverage again.

**Priority:** P2 — existing render comparison fails independently of the product cleanup

- [x] Reproduce the failure on the current and pre-cleanup product revisions.
- [ ] Investigate reference provenance and fixture stability; distinguish intended visual changes from defects before changing a golden.
- [ ] Restore the reviewed City render acceptance on available platforms and record Windows/Metal outcomes.
- [ ] Run the MegaCity aggregate, same-cache smoke and City render comparison for the completed fix.

2026-10-09: `ctest --test-dir build --build-config Release --parallel 1 --no-tests=error --output-on-failure -R '^draxul-render-megacity-plugin$'` failed on macOS/Metal with **82.6172% (507600 pixels)** drift both before and after the cleanup. The pre-cleanup source was restored at `06c804c11cdda5de6e2c5d986fdbe33c1e85da1b`, rebuilt in the same Release/Makefiles cache, and exercised with its original test sources. Then the cleanup patch was reapplied and rebuilt. The two actual BMPs are byte-identical (SHA-256 `c25d1ae91649627187beadff639d626a5d21ff8ba18cef92de21ecdcfdd2c2ec`). This is evidence of an inherited reference mismatch, not permission to bless it.

The fixture scans the product's own test directory. Reassess whether that is sufficiently stable for a golden before adopting a reference. Baseline render took 16.36s; cleanup render took 16.11s. Logs: `/tmp/draxul-retire-baseline-render.log` and `/tmp/draxul-retire-city-render.log`; comparison image/report: `/tmp/draxul-retire-city-after.bmp` and `/tmp/draxul-retire-city-after.json`. Golden files were left unchanged. Removal validation is recorded in `kanban/done/17 remove-archived-biology-mode -refactor.md`.
