# Own MegaCity grid and route workers privately

**Summary:** Give MegaCity's background city-building and route tasks separate owners so cancellation and shutdown can be tested without exposing thread details throughout the host.

**Priority:** P1 — two worker lifecycles crowd the public host header and timing tests.  
**Source:** `plugins/megacity/product/draxul-megacity/include/draxul/megacity_host.h`  
**Proposed by:** Claude 43; Codex 3. **Owner:** one MegaCity agent.  
**Evidence:** header stores both thread/lock/generation sets; route worker calls host callback; tests use `#define private public` and sleeps.

**Boundary verification**
- [ ] Record cancellation, stale-generation, callback lifetime and nonblocking retirement behavior.
**Implementation and migration**
- [ ] Extract private grid and route worker owners with immutable requests/results; migrate grid then route. Keep algorithms in model and scene publication in host.
**Unit tests**
- [ ] Inject blocked builders for cancellation/latest-result/shutdown; retain host integration.
**Cross-platform validation**
- [ ] Check Windows/macOS thread teardown, `do.py test debug --megacity`, same-cache smoke and City panes.
**Agent documentation and tooling**
- [ ] Update MegaCity guide at product root and focused test descriptions.
**Acceptance criteria**
- [ ] Host no longer exposes worker machinery; pump/input remain nonblocking.
