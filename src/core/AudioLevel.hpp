#pragma once

namespace screencam::core {

/// Maps a peak sample amplitude (0..1, linear) to a 0..1 meter position on a decibel scale.
/// Ears hear loudness logarithmically, so a linear bar would sit near zero for normal speech:
/// -60 dBFS (barely audible) -> 0.0, 0 dBFS (clipping) -> 1.0, -20 dBFS (speech) -> ~0.67.
double peakToMeterLevel(double peak);

/// Falling-bar smoothing: jumps up immediately, decays by `decay` (0..1) per update.
double smoothMeterLevel(double previous, double current, double decay);

}  // namespace screencam::core
