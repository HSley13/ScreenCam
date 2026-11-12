#pragma once

// The editor's timeline: the video is cut into consecutive segments, each with its own camera
// bubble layout. Only the segments' start times matter here — segment i runs from starts[i]
// until starts[i + 1] (or the end of the video). starts[0] is always 0 and starts is sorted.

#include <cstdint>
#include <vector>

namespace screencam::core {

/// Index of the segment playing at `timeMs` (the last one starting at or before it).
/// Times before 0 map to segment 0; an empty list returns -1.
int segmentAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs);

/// Whether a new cut at `timeMs` would leave every segment at least `minLengthMs` long
/// (cuts too close to an existing one, or to the start/end, would make unusable slivers).
bool canSplitAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs, std::int64_t durationMs,
                std::int64_t minLengthMs);

/// Where a cut at `timeMs` goes in `starts` (keeps the list sorted).
std::size_t insertionIndex(const std::vector<std::int64_t> &starts, std::int64_t timeMs);

}  // namespace screencam::core
