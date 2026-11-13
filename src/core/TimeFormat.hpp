#pragma once

#include <string>

namespace screencam::core {

/// "00:42", "12:03", "1:02:03"; negative or NaN counts as zero.
std::string formatElapsed(double seconds);

}  // namespace screencam::core
