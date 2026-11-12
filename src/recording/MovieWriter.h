#import <AVFoundation/AVFoundation.h>

#include "core/EncoderPlan.hpp"
#include "core/Resolution.hpp"

NS_ASSUME_NONNULL_BEGIN

/// Encodes one .mov file (one video track + optional AAC audio track) from live sample buffers.
///
/// Thread safety: append methods may be called from different capture queues at the same
/// time (screen frames, camera frames, mic audio); a lock serializes them.
///
/// Timeline: the movie starts at the timestamp of the *first video frame*. Audio that arrives
/// earlier is dropped so the file doesn't open on a black frame with sound.
@interface MovieWriter : NSObject

@property (nonatomic, readonly) NSURL *url;
/// Set when finishing failed (or nothing was captured). Read it after the finish completion.
@property (nonatomic, readonly, nullable) NSError *finishError;

/// `size` is the output size. Incoming frames of another size are scaled to *fill* it and the
/// overflow is cropped from the centre — that is how the portrait camera file is cut out of
/// a landscape webcam without any manual pixel work.
- (nullable instancetype)initWithURL:(NSURL *)url
                                size:(screencam::core::PixelSize)size
                                plan:(screencam::core::EncoderPlan)plan
                        includeAudio:(BOOL)includeAudio
                               error:(NSError **)error;

- (void)appendVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)appendAudioSampleBuffer:(CMSampleBufferRef)sampleBuffer;

/// Ends the movie at `endTime` (host clock) and flushes it to disk. `completion` runs on an
/// arbitrary queue; afterwards check `finishError`.
- (void)finishAtSourceTime:(CMTime)endTime completion:(void (^)(void))completion;

/// Abandons the file (used when starting the recording failed).
- (void)cancel;

@end

NS_ASSUME_NONNULL_END
