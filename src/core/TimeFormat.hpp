#pragma once

#include <string>

namespace screencam::core {

/// Formats a recording duration for the status label: "00:42", "12:03", "1:02:03".
/// Negative or NaN input is treated as zero.
std::string formatElapsed(double seconds);

}  // namespace screencam::core
