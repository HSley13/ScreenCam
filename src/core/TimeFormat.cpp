#include "core/TimeFormat.hpp"

#include <cstdio>

namespace screencam::core {

std::string formatElapsed(double seconds) {
    if (!(seconds > 0.0)) {  // also catches NaN
        seconds = 0.0;
    }
    const long long total = static_cast<long long>(seconds);
    const long long hours = total / 3600;
    const long long minutes = (total % 3600) / 60;
    const long long secs = total % 60;

    char buffer[32];
    if (hours > 0) {
        std::snprintf(buffer, sizeof(buffer), "%lld:%02lld:%02lld", hours, minutes, secs);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld", minutes, secs);
    }
    return buffer;
}

}  // namespace screencam::core
