#import <CoreMedia/CoreMedia.h>

CF_ASSUME_NONNULL_BEGIN

/// Why this exists: three sources feed one movie — ScreenCaptureKit (timestamps on the
/// *host clock*), the camera session and the mic session (timestamps on each session's
/// `synchronizationClock`). AVAssetWriter puts samples on the timeline purely by timestamp,
/// so all of them must share one clock or audio drifts away from the picture.
///
/// Returns a +1 retained sample buffer whose timestamps are converted from `sourceClock` to
/// the host clock (just a retained copy of the input when they are already the same clock).
/// The caller must CFRelease the result. Returns NULL only if the copy fails.
CMSampleBufferRef _Nullable SCCopySampleBufferOnHostClock(CMSampleBufferRef sampleBuffer,
                                                          CMClockRef _Nullable sourceClock) CF_RETURNS_RETAINED;

CF_ASSUME_NONNULL_END
