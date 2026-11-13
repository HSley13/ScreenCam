#pragma once

// The editor's timeline: consecutive segments, each with its own bubble layout. Segment i
// runs from starts[i] to starts[i + 1]; starts is sorted and starts[0] is 0.

#include <cstdint>
#include <vector>

namespace screencam::core {

/// The segment playing at `timeMs`; -1 for an empty list.
int segmentAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs);

/// Whether a cut at `timeMs` leaves every segment at least `minLengthMs` long.
bool canSplitAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs, std::int64_t durationMs,
                std::int64_t minLengthMs);

std::size_t insertionIndex(const std::vector<std::int64_t> &starts, std::int64_t timeMs);

}  // namespace screencam::core
