# Extract Megacity model, layout, grid, and routing library

**Type:** refactor
**Priority:** P1 / sequence 05
**Raised by:** Claude and GPT/Codex
**Consensus:** `plans/reviews/review-refactor-consensus.md`, Accepted 6

## Goal

Create one CPU-only `draxul-megacity-model` static library with focused internal
model/layout/grid/routing implementation files and direct deterministic tests.

## Boundary verification

- [x] Classify every record/function in `semantic_city_layout.h/.cpp` as semantic
  model, spatial layout, grid, routing, or downstream scene/presentation behavior.
- [x] Inventory all production/test consumers and required public value types.
- [x] Identify the subset of `MegaCityCodeConfig` actually consumed by model/layout/routing.
- [x] Define deterministic ordering, stable identity, route, and geometry-index invariants.
- [x] Confirm host worker cancellation/publication stays outside the proposed library.

## Implementation and migration

- [x] Introduce a narrow `SemanticCityLayoutOptions` value and an adapter from `MegaCityCodeConfig`.
- [x] Add module-owned public model/layout/grid/route headers with immutable records/functions.
- [x] Create `draxul-megacity-model` initially around the existing single implementation TU.
- [x] Link `draxul-megacity` and focused tests directly to the new target.
- [x] Split implementation into semantic model, spatial layout, city grid, and routing TUs.
- [x] Keep pathfinding queues, lot grids, simplification, and helper algorithms private.
- [x] Narrow `draxul-megacity-test-internals` after model tests stop needing the broad product target.

## Unit tests

- [x] Move semantic projection tests to direct model-target linkage.
- [x] Pin deterministic lot/module/city placement and stable identity.
- [x] Pin grid rasterization and occupied/sidewalk/road classification.
- [x] Pin route ports, pathfinding, duplicate-name identity, and segment conversion.
- [x] Build `draxul-megacity-model`, `draxul-test-megacity-model`, and `draxul-test-megacity`; run CTest label `megacity` after the final TU split.
- [x] Add a public-header/link-isolation consumer for the model target.

## Cross-platform validation

- [ ] Configure/build MegaCity ON and OFF on Windows and macOS.
- [x] Confirm CPU outputs and ordering are identical for Vulkan and Metal consumers: both backends consume the same backend-neutral model records.
- [x] Preserve 16-bit downstream `GeometryMesh` index assumptions; the extraction does not change scene mesh records or geometry generation.
- [x] Run the existing MegaCity host/scene tests and launch the host on an available backend.
- [x] Record other-platform runtime validation if only one backend is available. (Metal/Apple M5 passed; Vulkan/Windows remains.)

## Agent documentation and tooling

- [x] Update the core `docs/module-map.md` and `plugins/megacity/product/AGENTS.md` with the new direction.
- [x] Document that model code is synchronous, deterministic, backend-neutral, and host-thread agnostic.
- [x] Update focused Megacity validation commands to current target/CTest label names.

## Acceptance criteria

- [x] `draxul-megacity-model` has no host, ImGui, SDL, font, Vulkan, or Metal dependency.
- [x] Model/layout/grid/routing tests link without the broad Megacity product/renderer closure.
- [x] One static link boundary is used; algorithms are not fragmented into micro-libraries.
- [x] Semantic layout, routing, host behavior, and rendered scene inputs remain equivalent.
- [ ] Focused/full tests, optional ON/OFF builds, and smoke pass.

## Dependencies and ownership

Depends on the core repository's internal-target build-policy work. One MegaCity
model owner freezes the public records/options and CMake target. Layout
and routing TU/test moves may then proceed independently without touching
renderer backends or host lifecycle code.

## Source-complete checkpoint (2026-09-22)

- MSVC syntax-only checks pass for the four model implementation units, the
  product config adapter, the direct model test source, and the remaining broad
  scene-layout test source.
- At this source-only checkpoint, build, CTest, optional ON/OFF configuration,
  smoke, and interactive host validation were deferred to the shared Draxul
  cache; the completed Windows gates are recorded below.

## Windows validation checkpoint (2026-09-22)

- The post-edit `py do.py test debug --products` gate passed all 48 selected
  entries in the shared Windows Debug/Ninja cache (230.53 s), including all
  three MegaCity entries, the direct model target, and the broad host/scene
  suite.
- The registered MegaCity Vulkan render passed, and the subsequent same-cache
  smoke passed.
- The product aggregate establishes the final-TU-split test and behavioral
  equivalence gates above. Optional MegaCity-OFF configuration and a current
  macOS build remain open.
