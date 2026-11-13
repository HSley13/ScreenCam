#pragma once

namespace screencam::core {

/// Peak amplitude (0..1, linear) to a 0..1 meter position on a dB scale: -60 dBFS -> 0,
/// 0 dBFS -> 1. Loudness is heard logarithmically; a linear bar would barely move for speech.
double peakToMeterLevel(double peak);

/// Falling-bar smoothing: jumps up immediately, decays by `decay` (0..1) per update.
double smoothMeterLevel(double previous, double current, double decay);

}  // namespace screencam::core
