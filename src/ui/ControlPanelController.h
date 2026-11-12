#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "core/CameraLayout.hpp"
#include "core/Resolution.hpp"

NS_ASSUME_NONNULL_BEGIN

@class ControlPanelController;

/// How the panel reports user actions. The panel only *shows* state; AppDelegate decides.
@protocol ControlPanelDelegate <NSObject>
- (void)controlPanelSelectionDidChange:(ControlPanelController *)panel;      // any picker changed
- (void)controlPanelDidRequestToggleRecording:(ControlPanelController *)panel;
- (void)controlPanelDidRequestReveal:(ControlPanelController *)panel;         // "Show in Finder"
@end

/// The small settings window: screen, resolution, camera, camera mode, microphone + Record.
/// It is excluded from the recording, so it can stay open while you record.
@interface ControlPanelController : NSWindowController

@property (nonatomic, weak, nullable) id<ControlPanelDelegate> delegate;

- (instancetype)init;

/// Re-reads cameras/mics/displays and refills the pop-ups, keeping the current choice
/// when that device still exists (called at launch and on every hot-plug).
- (void)reloadSources;

// Current selection. A nil camera / microphone id means "None".
@property (nonatomic, readonly) CGDirectDisplayID selectedDisplayID;
@property (nonatomic, readonly, nullable) NSString *selectedCameraID;
@property (nonatomic, readonly, nullable) NSString *selectedMicrophoneID;
@property (nonatomic, readonly) screencam::core::ResolutionPreset selectedResolution;
@property (nonatomic, readonly) screencam::core::CameraOrientation selectedOrientation;

/// Swaps the button title and locks the pickers while recording.
- (void)setRecording:(BOOL)recording;
/// Disables the Record/Stop button while a start or stop is in flight.
- (void)setBusy:(BOOL)busy;
- (void)setStatusText:(NSString *)text;
- (void)setRevealEnabled:(BOOL)enabled;

@end

NS_ASSUME_NONNULL_END
