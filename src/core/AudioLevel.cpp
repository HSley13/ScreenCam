#include "core/AudioLevel.hpp"

#include <algorithm>
#include <cmath>

namespace screencam::core {

namespace {
constexpr double kFloorDb = -60.0;
}  // namespace

double peakToMeterLevel(double peak) {
    if (!(peak > 0.0)) {  // also catches NaN
        return 0.0;
    }
    const double db = 20.0 * std::log10(std::min(peak, 1.0));
    return std::clamp((db - kFloorDb) / -kFloorDb, 0.0, 1.0);
}

double smoothMeterLevel(double previous, double current, double decay) {
    const double decayed = previous * (1.0 - std::clamp(decay, 0.0, 1.0));
    return std::max(current, decayed);
}

}  // namespace screencam::core
