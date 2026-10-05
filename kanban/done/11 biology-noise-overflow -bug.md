# Use defined arithmetic for biological noise

**Summary:** Use safe arithmetic when generating biological shapes so opening the biology view cannot cause invalid calculations.

**Priority:** 11  
**Severity:** CRITICAL  
**Source:** `plugins/megacity/product/draxul-geometry/src/cell_generator.cpp`

**Evidence and trigger:** B01; coordinate products overflow before unsigned conversion under ordinary biology settings.

- [x] **Investigate:** Trace default noise offsets, octave expansion, and negative-coordinate behavior.
- [x] **Fix:** Convert coordinates before multiplication and use unsigned constants.
- [x] **Acceptance:** Default biology construction and boundary coordinates execute without signed overflow and remain deterministic.
- [x] **Validation:** Run the MegaCity-scoped aggregate, relevant biology rendering checks, and same-cache smoke; use undefined-behavior instrumentation where available.

## Resolution

- **Confirmed (UBSan, `-fsanitize=undefined,float-cast-overflow`):** default
  `build_blob_mesh` reported `53 * 73856093` (and the `iy`/`iz` products)
  overflowing `int` in `lattice_value`. The default seed offset reaches ~65 per
  axis, the colour tint path scales it by 3 and fBm doubles it per octave, so
  lattice coordinates routinely exceed the +/-29 threshold. Boundary inputs also
  hit undefined float->int conversion and `INT_MAX + 1` corner offsets.
- **Fix (`product/draxul-geometry/src/cell_generator.cpp`):** lattice
  coordinates are now `uint32_t` two's-complement bit patterns; the hash
  products use unsigned constants and the corner offsets wrap with defined
  arithmetic. `lattice_coord()` clamps floored values to the int32 range (NaN
  maps to 0) before conversion. Bits are unchanged wherever the old signed math
  did not overflow and match the old wrapped values where it did, so existing
  biology scenes keep their shapes (harness outputs identical before/after).
- **Regression tests (`tests/draxul_geometry_tests.cpp`):** pinned noise values
  at overflowing coordinates, finite/bounded/deterministic noise and 8-octave
  fBm at int32 boundaries and out-of-range magnitudes, and deterministic default
  blob construction.
- **Validation (macOS Debug):** focused `[cell]` (7 cases) pass; UBSan harness
  clean after the fix; `do.py test debug --megacity` 59/60 with the single
  failure being the core `draxul-test-app-shard-0` (`app_dispatch_tests.cpp:453`,
  shared Neovim split), which does not involve MegaCity; same-cache smoke pass.
  `tests/render/bioview-plugin.toml` builds and renders the biology scene to
  finalize (no reference image exists to compare). The registered
  `draxul-render-megacity-plugin` (city mode, no cell generator) drifts 5.86%
  identically with and without this change, so it is a pre-existing difference.
