#include "core/Timeline.hpp"

#include <algorithm>
#include <cstdlib>

namespace screencam::core {

int segmentAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs) {
    if (starts.empty()) {
        return -1;
    }
    // First start strictly greater than t; the segment before it is the one playing.
    const auto after = std::upper_bound(starts.begin(), starts.end(), timeMs);
    if (after == starts.begin()) {
        return 0;
    }
    return static_cast<int>(std::distance(starts.begin(), after)) - 1;
}

bool canSplitAt(const std::vector<std::int64_t> &starts, std::int64_t timeMs, std::int64_t durationMs,
                std::int64_t minLengthMs) {
    if (timeMs < minLengthMs || timeMs > durationMs - minLengthMs) {
        return false;
    }
    for (const std::int64_t start : starts) {
        if (std::llabs(start - timeMs) < minLengthMs) {
            return false;
        }
    }
    return true;
}

std::size_t insertionIndex(const std::vector<std::int64_t> &starts, std::int64_t timeMs) {
    return static_cast<std::size_t>(
        std::distance(starts.begin(), std::upper_bound(starts.begin(), starts.end(), timeMs)));
}

}  // namespace screencam::core
