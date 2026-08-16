#pragma once

// The ONE "poll until a complete CodebaseSnapshot is published" waiter,
// built on the shared draxul::tests::wait_until primitive. The scanner and
// SemanticSourceController suites all funnel through here instead of carrying
// byte-identical copies of the loop.

#include "test_support.h"

#include <draxul/treesitter.h>

#include <chrono>
#include <memory>
#include <utility>

namespace draxul
{

// take() is any callable returning std::shared_ptr<const CodebaseSnapshot>.
// Returns the first complete snapshot, or the provider's latest snapshot
// (possibly incomplete or null) once the timeout budget is spent.
template <typename TakeSnapshot>
std::shared_ptr<const CodebaseSnapshot> wait_for_complete_snapshot_from(
    TakeSnapshot&& take,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    std::shared_ptr<const CodebaseSnapshot> snapshot;
    if (draxul::tests::wait_until(
            [&] {
                snapshot = take();
                return snapshot && snapshot->complete;
            },
            timeout))
    {
        return snapshot;
    }
    return take();
}

inline std::shared_ptr<const CodebaseSnapshot> wait_for_complete_snapshot(
    CodebaseScanner& scanner,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    return wait_for_complete_snapshot_from(
        [&scanner] { return scanner.snapshot(); }, timeout);
}

} // namespace draxul
