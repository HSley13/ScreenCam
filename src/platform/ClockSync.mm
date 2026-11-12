#import "platform/ClockSync.h"

#include <vector>

CMSampleBufferRef SCCopySampleBufferOnHostClock(CMSampleBufferRef sampleBuffer, CMClockRef sourceClock) {
    CMClockRef hostClock = CMClockGetHostTimeClock();
    if (sourceClock == nullptr || sourceClock == hostClock) {
        return (CMSampleBufferRef)CFRetain(sampleBuffer);  // common case on macOS: nothing to do
    }

    // 1) Ask how many timing entries the buffer has, then read them.
    CMItemCount count = 0;
    CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, 0, nullptr, &count);
    if (count <= 0) {
        return (CMSampleBufferRef)CFRetain(sampleBuffer);
    }
    std::vector<CMSampleTimingInfo> timing(static_cast<size_t>(count));
    if (CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, count, timing.data(), &count) != noErr) {
        return (CMSampleBufferRef)CFRetain(sampleBuffer);
    }

    // 2) Translate every timestamp from the session clock to the host clock.
    for (CMSampleTimingInfo &info : timing) {
        if (CMTIME_IS_VALID(info.presentationTimeStamp)) {
            info.presentationTimeStamp = CMSyncConvertTime(info.presentationTimeStamp, sourceClock, hostClock);
        }
        if (CMTIME_IS_VALID(info.decodeTimeStamp)) {
            info.decodeTimeStamp = CMSyncConvertTime(info.decodeTimeStamp, sourceClock, hostClock);
        }
    }

    // 3) Make a shallow copy (pixels/audio data are shared, not duplicated) with the new timing.
    CMSampleBufferRef copy = nullptr;
    if (CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, sampleBuffer, count, timing.data(), &copy) != noErr) {
        return nullptr;
    }
    return copy;
}
